/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "elf/base/game_base.h"
#include "elf/distributed/shared_reader.h"

#include "record.h"

class ServerGame {
 public:
  ServerGame(
      int game_idx,
      const GameOptions& options,
      elf::shared::ReaderQueuesT<Record>* reader);

  void OnAct(elf::game::Base* base);

 private:
  elf::shared::ReaderQueuesT<Record>* reader_ = nullptr;
};
