#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "distri_base.h"

#include "../common/record.h"
#include "data_loader.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "game_ctrl.h"

#include <thread>

class Server {
 public:
  Server(
      const ContextOptions& contextOptions,
      const GameOptions& options,
      elf::GameClient* client)
      : contextOptions_(contextOptions),
        options_(options),
        logger_(elf::logging::getLogger("Server-", "")) {
    auto netOptions = getNetOptions(contextOptions_, options_);

    trainCtrl_.reset(new TrainCtrl(
        ctrl_,
        contextOptions_.num_games,
        client,
        options,
        contextOptions_.mcts_options));

    if (options_.mode == "train") {
      onlineLoader_.reset(new DataOnlineLoader(netOptions));
      onlineLoader_->start(trainCtrl_.get());
    } else if (options_.mode == "offline_train") {
    } else {
      throw std::range_error("options.mode not recognized! " + options_.mode);
    }
  }

  ReplayBuffer* getReplayBuffer() {
    return trainCtrl_->getReplayBuffer();
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

  ~Server() {
    trainCtrl_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

  void loadOfflineSelfplayData() {
    if (options_.list_files.empty())
      return;

    std::atomic<int> count(0);
    const size_t numThreads = 16;

    auto thread_main = [this, &count](size_t idx) {
      for (size_t k = 0; k * numThreads + idx < options_.list_files.size();
           ++k) {
        const std::string& f = options_.list_files[k * numThreads + idx];
        logger_->info("Loading offline data, reading file {}", f);

        std::string content;
        if (!Record::loadContent(f, &content)) {
          logger_->error("Offline data loader: error reading {}", f);
          return;
        }
        trainCtrl_->OnReceive("", content);
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
        options_.list_files.size(),
        trainCtrl_->getReplayBuffer()->info());
  }

 private:
  Ctrl ctrl_;

  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<DataOnlineLoader> onlineLoader_;

  const ContextOptions contextOptions_;
  const GameOptions options_;

  std::shared_ptr<spdlog::logger> logger_;
};
