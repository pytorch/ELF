/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "game_train.h"

GoGameTrain::GoGameTrain(
    int game_idx,
    elf::GameClient* client,
    const ContextOptions& context_options,
    const GameOptions& options,
    TrainCtrl* train_ctrl,
    elf::shared::ReaderQueuesT<Record>* reader)
    : GoGameBase(game_idx, client, context_options, options),
      train_ctrl_(train_ctrl),
      reader_(reader),
      _state_ext(game_idx, options) {}

void GoGameTrain::act() {
  // Train a model directly.
  while (true) {
    int q_idx = _rng() % reader_->nqueue();
    /*
    static mutex s_mutex;
    {
    lock_guard<mutex> lock(s_mutex);
    std::cout << "[" << _game_idx << "] Get Sampler, q_idx: " << q_idx << "/" <<
    _reader->nqueue() << std::endl;
    }
    */
    // cout << "[" << _game_idx << "] Before get sampler " << endl;
    auto sampler = reader_->getSampler(q_idx, &_rng);
    const Record* r = sampler.sample();
    // std::cout << "[" << _game_idx << "] Get Sampler, q_idx: " << q_idx << "/"
    // << _reader->nqueue() << std::endl;
    if (r == nullptr) {
      // std::cout << "No data, wait.." << endl;
      continue;
    }
    // std::cout << "Has data.." << endl;
    _state_ext.fromRecord(*r);

    // Random pick one ply.
    if (_state_ext.switchRandomMove(&_rng))
      break;
  }

  // std::cout << "[" << _game_idx << "] Generating D4Code.." << endl;
  _state_ext.generateD4Code(&_rng);

  elf::FuncsWithState funcs =
      client_->BindStateToFunctions({"train"}, &_state_ext);
  client_->sendWait({"train"}, &funcs);
}
