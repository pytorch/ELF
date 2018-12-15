/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../common/go_game_specific.h"
#include "../common/go_state_ext.h"
#include "../common/record.h"
#include "elf/interface/game_base.h"
#include "elf/distributed/shared_reader.h"

class GoGameTrain {
 public:
  GoGameTrain(
      int game_idx,
      const GameOptionsTrain& options,
      elf::shared::ReaderQueuesT<Record>* reader);

  void OnAct(elf::game::Base* base);

 private:
  elf::shared::ReaderQueuesT<Record>* reader_ = nullptr;

  static constexpr size_t kNumState = 64;
  std::vector<std::unique_ptr<GoStateExtOffline>> _state_ext;
};
