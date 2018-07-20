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
    elf::shared::ReaderQueuesT<Record>* reader)
    : GoGameBase(game_idx, client, context_options, options), reader_(reader) {
  for (size_t i = 0; i < kNumState; ++i) {
    _state_ext.emplace_back(new GoStateExtOffline(game_idx, options));
  }
}

void GoGameTrain::act() {
  std::vector<elf::FuncsWithState> funcsToSend;

  for (size_t i = 0; i < kNumState; ++i) {
    while (true) {
      // std::cout << "[" << _game_idx << "][" << i << "] Before get sampler "
      // << std::endl;
      int q_idx;
      auto sampler = reader_->getSamplerWithParity(&_rng, &q_idx);
      const Record* r = sampler.sample();
      // std::cout << "[" << _game_idx << "] Get Sampler, q_idx: " << q_idx <<
      // "/" << reader_->nqueue() << std::endl;
      if (r == nullptr) {
        // std::cout << "[" << _game_idx << "][" << i << "] No data, wait.." <<
        // std::endl;
        continue;
      }
      // std::cout << "[" << _game_idx << "][" << i << "] Has data.." <<
      // std::endl;
      _state_ext[i]->fromRecord(*r);

      // Random pick one ply.
      if (_state_ext[i]->switchRandomMove(&_rng))
        break;
    }

    // std::cout << "[" << _game_idx << "] Generating D4Code.." << endl;
    _state_ext[i]->generateD4Code(&_rng);

    // elf::FuncsWithState funcs =
    // client_->BindStateToFunctions({"train"}, &_state_ext);
    funcsToSend.push_back(
        client_->BindStateToFunctions({"train"}, _state_ext[i].get()));
  }

  // client_->sendWait({"train"}, &funcs);

  std::vector<elf::FuncsWithState*> funcPtrsToSend(funcsToSend.size());
  for (size_t i = 0; i < funcsToSend.size(); ++i) {
    funcPtrsToSend[i] = &funcsToSend[i];
  }

  // VERY DANGEROUS - sending pointers of local objects to a function
  // std::cout << "[" << _game_idx << "] Sending packages to python.." <<
  // std::endl;
  client_->sendBatchWait({"train"}, funcPtrsToSend);
  // std::cout << "[" << _game_idx << "] Return from python ..." << std::endl;
}
