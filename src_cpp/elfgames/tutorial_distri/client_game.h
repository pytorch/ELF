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

#include "options.h"
#include "record.h"
#include "state.h"
#include "dispatcher_callback.h"

class ClientGame {
 public:
  using ThreadedDispatcher = elf::ThreadedDispatcherT<MsgRequest, MsgReply>;
  ClientGame(
      int game_idx,
      const GameOptions& options,
      ThreadedDispatcher* dispatcher);

  void OnAct(elf::game::Base* base);
  void OnEnd(elf::game::Base*) { }
  bool OnReceive(const MsgRequest& request, MsgReply* reply);

 private:
  ThreadedDispatcher* dispatcher_ = nullptr;
  int counter_ = 0;
  State state_;

  // used to communicate info.
  elf::game::Base* base_ = nullptr;

  const GameOptions options_;
};
