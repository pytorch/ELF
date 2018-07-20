/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../common/game_base.h"
#include "elf/distributed/shared_reader.h"

class GoGameTrain : public GoGameBase {
 public:
  GoGameTrain(
      int game_idx,
      elf::GameClient* client,
      const ContextOptions& context_options,
      const GameOptions& options,
      elf::shared::ReaderQueuesT<Record>* reader);

  void act() override;

 private:
  elf::shared::ReaderQueuesT<Record>* reader_ = nullptr;

  static constexpr size_t kNumState = 64;
  std::vector<std::unique_ptr<GoStateExtOffline>> _state_ext;
};
