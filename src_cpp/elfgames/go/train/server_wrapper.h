#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "../base/board_feature.h"
#include "../common/game_feature.h"
#include "../common/record.h"
#include "elf/base/game_context.h"
#include "elf/distributed/data_loader.h"
#include "elf/distributed/addrs.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "game_ctrl.h"
#include "game_train.h"

#include "elf/distri/server.h"

using elf::cs::ServerInterface;
using elf::cs::ServerGame;
using elf::cs::Records;
using elf::cs::ReplayBuffer;
using elf::Extractor;
using elf::cs::Server;

class ServerWrapper : public ServerInterface {
 public:
  ServerWrapper(const GameOptionsTrain& options)
      : options_(options), selfplay_record_("tc_selfplay"),
        goFeature_(options.common.use_df_feature, options.num_future_actions),
        logger_(elf::logging::getLogger("Server-", "")) {}

  void set(Server *server) {
    assert(server);
    server_ = server;

    auto *ctx = server_->ctx();

    threaded_ctrl_.reset(
        new ThreadedCtrl(
          ctrl_, 
          ctx->getClient(), 
          server_->getReplayBuffer(), 
          options_));

    ctx->getExtractor().merge(goFeature_.registerExtractor(ctx->options().batchsize));
    server_->setInterface(this);
  }

  void onStart() override { 
    // Call by shared_rw thread or any thread that will call OnReceive.
    ctrl_.reg("train_ctrl");
    ctrl_.addMailbox<int>();
    threaded_ctrl_->Start();
  }
  
  elf::shared::InsertInfo onReceive(Records &&rs, const ClientInfo& info) override {
    ReplayBuffer *replay_buffer = server_->getReplayBuffer();

    if (rs.identity.size() == 0) {
      // No identity -> offline data.
      for (auto& r : rs.records) {
        r.offline = true;
      }
    }

    std::vector<FeedResult> selfplay_res =
        threaded_ctrl_->onSelfplayGames(rs.records);

    elf::shared::InsertInfo insert_info;
    for (size_t i = 0; i < rs.records.size(); ++i) {
      if (selfplay_res[i] == FeedResult::FEEDED ||
          selfplay_res[i] == FeedResult::VERSION_MISMATCH) {
        const Record& r = rs.records[i];

        Result result = Result::createFromJson(r.result.reply);
        bool black_win = result.reward > 0;
        insert_info +=
            replay_buffer->InsertWithParity(Record(r), &rng_, black_win);
        selfplay_record_.feed(r);
        selfplay_record_.saveAndClean(1000);
      }
    }

    std::vector<FeedResult> eval_res =
        threaded_ctrl_->onEvalGames(info, rs.records);
    threaded_ctrl_->checkNewModel(server_->getClientManager());

    recv_count_++;
    if (recv_count_ % 1000 == 0) {
      int valid_selfplay = 0, valid_eval = 0;
      for (size_t i = 0; i < rs.records.size(); ++i) {
        if (selfplay_res[i] == FeedResult::FEEDED)
          valid_selfplay++;
        if (eval_res[i] == FeedResult::FEEDED)
          valid_eval++;
      }

      std::cout << "TrainCtrl: Receive data[" << recv_count_ << "] from "
                << rs.identity << ", #state_update: " << rs.states.size()
                << ", #records: " << rs.records.size()
                << ", #valid_selfplay: " << valid_selfplay
                << ", #valid_eval: " << valid_eval << std::endl;
    }
    return insert_info;
  }

  void fillInRequest(const ClientInfo &info, MsgRequest *request) override {
    threaded_ctrl_->fillInRequest(info, request);
  }

  ServerGame *createGame(int idx) override {
    auto *p = new GoGameTrain(idx, options_);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      games_.emplace_back(p);
    }

    return p;
  } 

  bool setEvalMode(int64_t new_ver, int64_t old_ver) {
    //std::cout << "Setting eval mode: new: " << new_ver << ", old: " << old_ver
    //          << std::endl;
    logger_->info("Setting eval mode: new: {}, old: {}", new_ver, old_ver);
    // Eval only.
    server_->getClientManager()->setClientTypeRatio({0.0f, 1.0f});
    threaded_ctrl_->setEvalMode(new_ver, old_ver);
    return true;
  }

  void waitForSufficientSelfplay(int64_t selfplay_ver) {
    threaded_ctrl_->waitForSufficientSelfplay(selfplay_ver);
  }

  // Used in training side.
  void notifyNewVersion(int64_t selfplay_ver, int64_t new_version) {
    threaded_ctrl_->addNewModelForEvaluation(
        selfplay_ver, new_version);
  }

  void setInitialVersion(int64_t init_version) {
    threaded_ctrl_->setInitialVersion(init_version);
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

 private:
  const GameOptionsTrain options_;

  Ctrl ctrl_;
  std::unique_ptr<ThreadedCtrl> threaded_ctrl_;

  int recv_count_ = 0;
  std::mt19937 rng_;

  // SelfCtrl has its own record buffer to save EVERY game it has received.
  RecordBufferSimple selfplay_record_;

  std::mutex mutex_;
  std::vector<std::unique_ptr<GoGameTrain>> games_;

  Server *server_ = nullptr;
  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;

  void loadOfflineSelfplayData() {
    const auto& list_files = options_.list_files;

    if (list_files.empty())
      return;

    std::atomic<int> count(0);
    const size_t numThreads = 16;

    auto thread_main = [this, &count, &list_files](size_t idx) {
      for (size_t k = 0; k * numThreads + idx < list_files.size(); ++k) {
        const std::string& f = list_files[k * numThreads + idx];
        logger_->info("Loading offline data, reading file {}", f);

        std::string content;
        if (!Record::loadContent(f, &content)) {
          logger_->error("Offline data loader: error reading {}", f);
          return;
        }
        elf::shared::InsertInfo info = server_->getDataHolder()->OnReceive("", content);
        count += info.n;
      }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
      threads.emplace_back(std::bind(thread_main, i));
    }

    for (auto& t : threads) {
      t.join();
    }

    logger_->info(
        "All offline data is loaded. Read {} records from {} files. Reader "
        "info {}",
        count,
        list_files.size(),
        server_->getReplayBuffer()->info());
  }
};
