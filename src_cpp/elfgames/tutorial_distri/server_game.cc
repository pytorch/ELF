/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "server_game.h"

ServerGame::ServerGame(
    int game_idx,
    const GameOptions& options,
    elf::shared::ReaderQueuesT<Record>* reader)
    : reader_(reader) {
}

void ServerGame::OnAct(elf::game::Base* base) {
  std::vector<elf::FuncsWithState> funcsToSend;

  for (size_t i = 0; i < kNumState; ++i) {
    while (true) {
      int q_idx;
      auto sampler = reader_->getSamplerWithParity(&base->rng(), &q_idx);
      const Record* r = sampler.sample();
      if (r == nullptr) {
        continue;
      }
      _state_ext[i]->fromRecord(*r);

      // Random pick one ply.
      if (_state_ext[i]->switchRandomMove(&base->rng()))
        break;
    }

    _state_ext[i]->generateD4Code(&base->rng());

    // elf::FuncsWithState funcs =
    // client_->BindStateToFunctions({"train"}, &_state_ext);
    funcsToSend.push_back(base->ctx().client->BindStateToFunctions(
        {"train"}, _state_ext[i].get()));
  }

  // client_->sendWait({"train"}, &funcs);

  std::vector<elf::FuncsWithState*> funcPtrsToSend(funcsToSend.size());
  for (size_t i = 0; i < funcsToSend.size(); ++i) {
    funcPtrsToSend[i] = &funcsToSend[i];
  }

  base->ctx().client->sendBatchWait({"train"}, funcPtrsToSend);
}
