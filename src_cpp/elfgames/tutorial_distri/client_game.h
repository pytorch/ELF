/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <random>
#include <string>

#include "elf/base/dispatcher.h"
#include "elf/base/game_base.h"

class ClientGame {
 public:
  using ThreadedDispatcher = elf::ThreadedDispatcherT<MsgRequest, MsgReply>;
  ClientGame(
      int game_idx,
      const GameOptions& options,
      ThreadedDispatcher* dispatcher);

  void OnAct(elf::game::Base* base);
  void OnEnd(elf::game::Base*) { }
  bool OnReceive(const MsgRequest& request, RestartReply* reply);

 private:
  ThreadedDispatcher* dispatcher_ = nullptr;
  int _online_counter = 0;

  // used to communicate info.
  elf::game::Base* base_ = nullptr;

  const GameOptions options_;
};
