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
#include "elf/base/game_context.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include <thread>

class Client {
 public:
  Client(const GameOptionsSelfPlay& options)
      : options_(options), goFeature_(options.common.use_df_feature, 1) {}

  void setGameContext(elf::GameContext* context) {
    // Only works for online setting.
    if (options_.common.mode != "online") {
      throw std::range_error(
          "options.mode not recognized! " + options_.common.mode);
    }

    const int numGames = options_.common.base.num_game_thread;
    const int batchsize = options_.common.base.batchsize;

    dispatcher_.reset(new ThreadedDispatcher(ctrl_, numGames));
    dispatcher_callback_.reset(
        new DispatcherCallback(dispatcher_.get(), context->getClient()));

    // Register all functions.
    goFeature_.registerExtractor(
        batchsize,
        context->getCollectorContext()->getCollectors()->getExtractor());

    for (int i = 0; i < numGames; ++i) {
      games_.emplace_back(new GoGameSelfPlay(i, options_, dispatcher_.get()));
      context->getGame(i)->setCallbacks(
          [&, i](elf::game::Base* base) { games_[i]->OnAct(base); },
          [&, i](elf::game::Base*) { dispatcher_->RegGame(i); });
    }
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

  const GoGameSelfPlay* getGameC(int idx) const {
    return games_[idx].get();
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
    request.vers.mcts_opt = options_.common.mcts;
    request.client_ctrl.resign_thres = thres;
    request.client_ctrl.num_game_thread_used = numThreads;
    dispatcher_->sendToThread(request);
  }

  ~Client() {
    dispatcher_.reset(nullptr);
    dispatcher_callback_.reset(nullptr);
  }

 private:
  const GameOptionsSelfPlay options_;
  GoFeature goFeature_;
  Ctrl ctrl_;

  std::vector<std::unique_ptr<GoGameSelfPlay>> games_;
  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<DispatcherCallback> dispatcher_callback_;

  std::shared_ptr<spdlog::logger> logger_;
};
