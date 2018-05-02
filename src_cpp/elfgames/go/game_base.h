/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "elf/utils/utils.h"

#include "game_feature.h"

class GoGameBase {
 public:
  GoGameBase(
      int game_idx,
      elf::GameClient* client,
      const ContextOptions& context_options,
      const GameOptions& options)
      : client_(client),
        _game_idx(game_idx),
        _options(options),
        _context_options(context_options) {
    if (options.seed == 0) {
      _seed = elf_utils::get_seed(
          game_idx ^ std::hash<std::string>{}(context_options.job_id));
    } else {
      _seed = options.seed;
    }
    _rng.seed(_seed);
  }

  void mainLoop() {
    if (_options.verbose)
      std::cout << "[" << _game_idx << "] Seed:" << _seed
                << ", thread_id: " << std::this_thread::get_id() << std::endl;
    // Main loop of the game.
    while (!client_->DoStopGames()) {
      act();
    }
  }

  virtual void act() = 0;

  virtual ~GoGameBase() = default;

 protected:
  elf::GameClient* client_ = nullptr;
  uint64_t _seed = 0;
  std::mt19937 _rng;

  int _game_idx = -1;

  GameOptions _options;
  ContextOptions _context_options;
};
