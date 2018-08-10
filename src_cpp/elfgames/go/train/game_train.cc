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
      int q_idx;
      auto sampler = reader_->getSamplerWithParity(&_rng, &q_idx);
      const Record* r = sampler.sample();
      if (r == nullptr) {
        continue;
      }
      _state_ext[i]->fromRecord(*r);

      // Random pick one ply.
      if (_state_ext[i]->switchRandomMove(&_rng))
        break;
    }

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
  client_->sendBatchWait({"train"}, funcPtrsToSend);
}
