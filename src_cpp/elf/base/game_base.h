/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "elf/base/context.h"
#include "elf/base/ctrl.h"
#include "elf/utils/utils.h"

namespace elf {

struct Ctx {
  GameClient* client = nullptr;
  Ctrl ctrl;
};

namespace game {

struct Options {
  int game_idx;
  uint64_t seed;
  std::string job_id;
  bool verbose = false;
};

class Base {
 public:
  using StartFunc = std::function<void(Base*)>;
  using ActFunc = std::function<void(Base*)>;
  using EndFunc = std::function<void(Base*)>;

  Base(Ctx& ctx, const Options& options) : ctx_(ctx), options_(options) {
    if (options_.seed == 0) {
      options_.seed = elf_utils::get_seed(
          options_.game_idx ^ std::hash<std::string>{}(options_.job_id));
    }
    rng_.seed(options_.seed);
  }

  void mainLoop() {
    assert(act_func_ != nullptr);
    if (options_.verbose)
      //std::cout << "[" << options_.game_idx << "] Seed:" << options_.seed
      //          << ", thread_id: " << std::this_thread::get_id() << std::endl;
    if (start_func_ != nullptr)
      start_func_(this);

    // Main loop of the game.
    while (!ctx_.client->DoStopGames()) {
      act_func_(this);
    }
    if (end_func_ != nullptr)
      end_func_(this);
  }

  void setCallbacks(
      ActFunc func,
      EndFunc end_func = nullptr,
      StartFunc start_func = nullptr) {
    start_func_ = start_func;
    act_func_ = func;
    end_func_ = end_func;
  }

  elf::Ctx& ctx() {
    return ctx_;
  }
  std::mt19937& rng() {
    return rng_;
  }
  const Options& options() const {
    return options_;
  }

 protected:
  elf::Ctx& ctx_;
  std::mt19937 rng_;
  Options options_;

  StartFunc start_func_ = nullptr;
  ActFunc act_func_ = nullptr;
  EndFunc end_func_ = nullptr;
};

} // namespace game

} // namespace elf
