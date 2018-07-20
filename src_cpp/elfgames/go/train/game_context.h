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

#include "../base/board_feature.h"
#include "../common/game_selfplay.h"
#include "../common/record.h"
#include "../mcts/ai.h"
#include "data_loader.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "game_train.h"

#include "distri_client.h"
#include "distri_server.h"

#include <thread>

class GameContext {
 public:
  using ThreadedDispatcher = GoGameSelfPlay::ThreadedDispatcher;

  GameContext(const ContextOptions& contextOptions, const GameOptions& options)
      : goFeature_(options),
        logger_(elf::logging::getLogger("elfgames::go::GameContext-", "")) {
    context_.reset(new elf::Context);

    int numGames = contextOptions.num_games;
    const int batchsize = contextOptions.batchsize;

    // Register all functions.
    goFeature_.registerExtractor(batchsize, context_->getExtractor());

    elf::GameClient* gc = context_->getClient();
    ThreadedDispatcher* dispatcher = nullptr;

    if (options.mode == "train" || options.mode == "offline_train") {
      server_.reset(new Server(contextOptions, options, gc));

      for (int i = 0; i < numGames; ++i) {
        games_.emplace_back(new GoGameTrain(
            i, gc, contextOptions, options, server_->getReplayBuffer()));
      }
    } else {
      client_.reset(new Client(contextOptions, options, gc));
      dispatcher = client_->getDispatcher();
      for (int i = 0; i < numGames; ++i) {
        games_.emplace_back(new GoGameSelfPlay(
            i,
            gc,
            contextOptions,
            options,
            dispatcher,
            client_->getNotifier()));
      }
    }

    context_->setStartCallback(
        numGames, [this, dispatcher](int i, elf::GameClient*) {
          if (dispatcher != nullptr) {
            dispatcher->RegGame(i);
          }
          games_[i]->mainLoop();
        });

    if (server_ != nullptr) {
      context_->setCBAfterGameStart(
          [this, options]() { server_->loadOfflineSelfplayData(); });
    }
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

  elf::Context* ctx() {
    return context_.get();
  }

  Server* getServer() {
    return server_.get();
  }
  Client* getClient() {
    return client_.get();
  }

  ~GameContext() {
    server_.reset(nullptr);
    client_.reset(nullptr);
    games_.clear();
    context_.reset(nullptr);
  }

 private:
  bool _check_game_idx(int game_idx) const {
    return game_idx < 0 || game_idx >= (int)games_.size();
  }

 private:
  std::unique_ptr<elf::Context> context_;
  std::vector<std::unique_ptr<GoGameBase>> games_;

  std::unique_ptr<Server> server_;
  std::unique_ptr<Client> client_;

  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;
};
