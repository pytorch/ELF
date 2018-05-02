/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "game_base.h"
#include "game_ctrl.h"

class GoGameTrain : public GoGameBase {
 public:
  GoGameTrain(
      int game_idx,
      elf::GameClient* client,
      const ContextOptions& context_options,
      const GameOptions& options,
      TrainCtrl* train_ctrl,
      elf::shared::ReaderQueuesT<Record>* reader);

  void act() override;

 private:
  TrainCtrl* train_ctrl_ = nullptr;
  elf::shared::ReaderQueuesT<Record>* reader_ = nullptr;

  GoStateExtOffline _state_ext;
};
