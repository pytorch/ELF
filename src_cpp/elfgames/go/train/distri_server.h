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

#include <thread>

class Server {
 public:
  Server(const GameOptionsTrain& options)
      : options_(options),
        goFeature_(options.common.use_df_feature, options.num_future_actions),
        logger_(elf::logging::getLogger("Server-", "")) {}

  void setGameContext(elf::GameContext* ctx) {
    goFeature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());

    size_t num_games = ctx->options().num_game_thread;
    trainCtrl_.reset(
        new TrainCtrl(ctrl_, num_games, ctx->getClient(), options_));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(
            new GoGameTrain(i, options_, trainCtrl_->getReplayBuffer()));
        g->setCallbacks(std::bind(&GoGameTrain::OnAct, games_[i].get(), _1));
      }
    }

    ctx->getCollectorContext()->setCBAfterGameStart(
        [this]() { loadOfflineSelfplayData(); });

    if (options_.common.mode == "train") {
      auto netOptions =
          elf::msg::getNetOptions(options_.common.base, options_.common.net);
      // 10s
      netOptions.usec_sleep_when_no_msg = 10000000;
      netOptions.usec_resend_when_no_msg = -1;
      onlineLoader_.reset(new elf::msg::DataOnlineLoader(netOptions));
      onlineLoader_->start(trainCtrl_.get());
    } else if (options_.common.mode == "offline_train") {
    } else {
      throw std::range_error(
          "options.mode not recognized! " + options_.common.mode);
    }
  }

  void waitForSufficientSelfplay(int64_t selfplay_ver) {
    trainCtrl_->getThreadedCtrl()->waitForSufficientSelfplay(selfplay_ver);
  }

  // Used in training side.
  void notifyNewVersion(int64_t selfplay_ver, int64_t new_version) {
    trainCtrl_->getThreadedCtrl()->addNewModelForEvaluation(
        selfplay_ver, new_version);
  }

  void setInitialVersion(int64_t init_version) {
    trainCtrl_->getThreadedCtrl()->setInitialVersion(init_version);
  }

  void setEvalMode(int64_t new_ver, int64_t old_ver) {
    trainCtrl_->setEvalMode(new_ver, old_ver);
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

  ~Server() {
    trainCtrl_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

 private:
  std::vector<std::unique_ptr<GoGameTrain>> games_;
  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<elf::msg::DataOnlineLoader> onlineLoader_;

  const GameOptionsTrain options_;

  GoFeature goFeature_;
  Ctrl ctrl_;

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
        elf::shared::InsertInfo info = trainCtrl_->OnReceive("", content);
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
        trainCtrl_->getReplayBuffer()->info());
  }
};
