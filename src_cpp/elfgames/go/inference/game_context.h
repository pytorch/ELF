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
#include "../common/dispatcher_callback.h"
#include "../common/game_selfplay.h"
#include "../common/record.h"
#include "../mcts/ai.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include <thread>

class GameContext {
 public:
  GameContext(const ContextOptions& contextOptions, const GameOptions& options)
      : contextOptions_(contextOptions),
        goFeature_(options),
        logger_(elf::logging::getLogger("GameContext-", "")) {
    context_.reset(new elf::Context);

    // Only works for online setting.
    if (options.mode != "online") {
      throw std::range_error("options.mode not recognized! " + options.mode);
    }

    const int numGames = contextOptions.num_games;

    dispatcher_.reset(new ThreadedDispatcher(ctrl_, numGames));
    dispatcher_callback_.reset(
        new DispatcherCallback(dispatcher_.get(), context_->getClient()));

    const int batchsize = contextOptions.batchsize;

    // Register all functions.
    goFeature_.registerExtractor(batchsize, context_->getExtractor());

    for (int i = 0; i < numGames; ++i) {
      games_.emplace_back(new GoGameSelfPlay(
          i,
          context_->getClient(),
          contextOptions,
          options,
          dispatcher_.get()));
    }

    context_->setStartCallback(numGames, [this](int i, elf::GameClient*) {
      if (dispatcher_ != nullptr)
        dispatcher_->RegGame(i);
      games_[i]->mainLoop();
    });
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
    dispatcher_->sendToThread(request);
  }

  elf::Context* ctx() {
    return context_.get();
  }

  ~GameContext() {
    context_.reset(nullptr);
  }

 private:
  bool _check_game_idx(int game_idx) const {
    return game_idx < 0 || game_idx >= (int)games_.size();
  }

 private:
  std::unique_ptr<elf::Context> context_;
  std::vector<std::unique_ptr<GoGameBase>> games_;

  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<DispatcherCallback> dispatcher_callback_;
  Ctrl ctrl_;

  ContextOptions contextOptions_;
  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;
};
