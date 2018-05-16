/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// TODO: Figure out how to remove this (ssengupta@fb)
#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "base/board_feature.h"
#include "data_loader.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "game_selfplay.h"
#include "game_train.h"
#include "mcts/ai.h"
#include "record.h"

#include <thread>

class GameContext {
 public:
  GameContext(const ContextOptions& contextOptions, const GameOptions& options)
      : contextOptions_(contextOptions),
        goFeature_(options),
        logger_(
            getLoggerFactory()->makeLogger("elfgames::go::GameContext-", "")) {
    context_.reset(new elf::Context);

    auto netOptions = getNetOptions(contextOptions, options);
    auto currTimestamp = time(NULL);

    bool performTraining = false;

    int numGames = contextOptions.num_games;

    if (options.mode == "selfplay") {
      writer_.reset(new elf::shared::Writer(netOptions));
      logger_->info("Writer info: {}", writer_->info());
      evalCtrl_.reset(new EvalCtrl(
          context_->getClient(), writer_.get(), options, numGames));

      logger_->info("Send ctrl with timestamp {}", currTimestamp);
      writer_->Ctrl(std::to_string(currTimestamp));
    } else if (options.mode == "online") {
      evalCtrl_.reset(new EvalCtrl(
          context_->getClient(), writer_.get(), options, numGames));
    } else if (options.mode == "train") {
      initReader(numGames, options, contextOptions.mcts_options);
      onlineLoader_.reset(new DataOnlineLoader(*reader_, netOptions));

      auto start_func = [&]() { trainCtrl_->RegRecordSender(); };

      auto replier = [&](elf::shared::Reader* reader,
                         const std::string& identity,
                         std::string* msg) -> bool {
        (void)reader;
        trainCtrl_->onReply(identity, msg);
        if (logger_->should_log(spdlog::level::level_enum::debug)) {
          logger_->debug(
              "Replier: about to send: recipient {}; msg {}; reader {}",
              identity,
              *msg,
              reader_->info());
        }
        return true;
      };

      onlineLoader_->start(start_func, replier);
      performTraining = true;

    } else if (options.mode == "offline_train") {
      initReader(numGames, options, contextOptions.mcts_options);
      offlineLoader_.reset(
          new DataOfflineLoaderJSON(*reader_, options.list_files));
      offlineLoader_->start();
      logger_->info("Offline train; reader info {}", reader_->info());
      trainCtrl_->RegRecordSender();
      performTraining = true;

    } else {
      throw std::range_error("options.mode not recognized! " + options.mode);
    }

    const int batchsize = contextOptions.batchsize;

    // Register all functions.
    goFeature_.registerExtractor(batchsize, context_->getExtractor());

    if (performTraining) {
      for (int i = 0; i < numGames; ++i) {
        games_.emplace_back(new GoGameTrain(
            i,
            context_->getClient(),
            contextOptions,
            options,
            trainCtrl_.get(),
            reader_.get()));
      }
    } else {
      for (int i = 0; i < numGames; ++i) {
        games_.emplace_back(new GoGameSelfPlay(
            i,
            context_->getClient(),
            contextOptions,
            options,
            evalCtrl_.get()));
      }
    }

    context_->setStartCallback(numGames, [this](int i, elf::GameClient*) {
      if (evalCtrl_ != nullptr)
        evalCtrl_->RegGame(i);
      games_[i]->mainLoop();
    });

    context_->setCBAfterGameStart(
        [this, options]() { loadOfflineSelfplayData(options); });
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

  const GoGameBase* getGame(int game_idx) const {
    if (_check_game_idx(game_idx)) {
      logger_->error("Invalid game_idx [{}]", game_idx);
      return nullptr;
    }

    return games_[game_idx].get();
  }

  GameStats* getGameStats() {
    return &evalCtrl_->getGameStats();
  }

  void waitForSufficientSelfplay(int64_t selfplay_ver) {
    trainCtrl_->waitForSufficientSelfplay(selfplay_ver);
  }

  // Used in training side.
  void notifyNewVersion(int64_t selfplay_ver, int64_t new_version) {
    trainCtrl_->addNewModelForEvaluation(selfplay_ver, new_version);
  }

  void setInitialVersion(int64_t init_version) {
    trainCtrl_->setInitialVersion(init_version);
  }

  void setEvalMode(int64_t new_ver, int64_t old_ver) {
    trainCtrl_->setEvalMode(new_ver, old_ver);
  }

  // Used in client side.
  void setRequest(
      int64_t black_ver,
      int64_t white_ver,
      float thres,
      int numThreads = -1) {
    MsgRequest request;
    request.vers.black_ver = black_ver;
    request.vers.white_ver = white_ver;
    request.vers.mcts_opt = contextOptions_.mcts_options;
    request.client_ctrl.black_resign_thres = thres;
    request.client_ctrl.white_resign_thres = thres;
    request.client_ctrl.num_game_thread_used = numThreads;
    evalCtrl_->sendRequest(request);
  }

  elf::Context* ctx() {
    return context_.get();
  }

  ~GameContext() {
    trainCtrl_.reset(nullptr);
    evalCtrl_.reset(nullptr);
    offlineLoader_.reset(nullptr);
    onlineLoader_.reset(nullptr);
    reader_.reset(nullptr);
    writer_.reset(nullptr);
    games_.clear();
    context_.reset(nullptr);
  }

 private:
  elf::shared::Options getNetOptions(
      const ContextOptions& contextOptions,
      const GameOptions& options) {
    elf::shared::Options netOptions;
    netOptions.addr =
        options.server_addr == "" ? "localhost" : options.server_addr;
    netOptions.port = options.port;
    netOptions.use_ipv6 = true;
    netOptions.verbose = options.verbose;
    netOptions.identity = contextOptions.job_id;

    return netOptions;
  }

  void loadOfflineSelfplayData(const GameOptions& options) {
    if (options.list_files.empty())
      return;

    std::atomic<int> count(0);
    const size_t numThreads = 16;

    auto thread_main = [&options, this, &count, numThreads](size_t idx) {
      for (size_t k = 0; k * numThreads + idx < options.list_files.size();
           ++k) {
        const std::string& f = options.list_files[k * numThreads + idx];
        logger_->info("Loading offline data, reading file {}", f);

        std::vector<Record> records;
        if (!Record::loadBatchFromJsonFile(f, &records)) {
          logger_->error("Offline data loader: error reading {}", f);
          return;
        }

        for (auto& r : records) {
          r.offline = true;
        }

        std::vector<FeedResult> res = trainCtrl_->onSelfplayGames(records);

        std::mt19937 rng(time(NULL));

        // If the record does not fit in trainCtrl_,
        // we should just send it directly to the replay buffer.
        for (size_t i = 0; i < records.size(); ++i) {
          if (res[i] == FeedResult::FEEDED ||
              res[i] == FeedResult::VERSION_MISMATCH) {
            bool black_win = records[i].result.reward > 0;
            reader_->InsertWithParity(std::move(records[i]), &rng, black_win);
            count++;
          }
        }
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
        options.list_files.size(),
        reader_->info());
  }

  void initReader(
      int numGames,
      const GameOptions& options,
      const elf::ai::tree_search::TSOptions& mcts_opt) {
    elf::shared::RQCtrl ctrl;
    ctrl.num_reader = options.num_reader;
    ctrl.ctrl.queue_min_size = options.q_min_size;
    ctrl.ctrl.queue_max_size = options.q_max_size;

    auto converter =
        [this](const std::string& s, std::vector<Record>* rs) -> bool {
      if (rs == nullptr)
        return false;
      try {
        trainCtrl_->onReceive(s);
        rs->clear();
        return true;
      } catch (...) {
        logger_->error("Data malformed! String is {}", s);
        return false;
      }
    };

    reader_.reset(new elf::shared::ReaderQueuesT<Record>(ctrl));
    trainCtrl_.reset(new TrainCtrl(
        numGames, context_->getClient(), reader_.get(), options, mcts_opt));
    reader_->setConverter(converter);
    logger_->info("Finished initializing reader {}", reader_->info());
  }

  bool _check_game_idx(int game_idx) const {
    return game_idx < 0 || game_idx >= (int)games_.size();
  }

  static elf::logging::IndexedLoggerFactory* getLoggerFactory() {
    static elf::logging::IndexedLoggerFactory factory(
        [](const std::string& name) { return spdlog::stderr_color_mt(name); });
    return &factory;
  }

 private:
  std::unique_ptr<elf::Context> context_;
  std::vector<std::unique_ptr<GoGameBase>> games_;

  ContextOptions contextOptions_;

  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<EvalCtrl> evalCtrl_;

  std::unique_ptr<elf::shared::Writer> writer_;
  std::unique_ptr<elf::shared::ReaderQueuesT<Record>> reader_;

  std::unique_ptr<DataOfflineLoaderJSON> offlineLoader_;
  std::unique_ptr<DataOnlineLoader> onlineLoader_;

  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;
};
