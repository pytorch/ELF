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
    (void)options;
    (void)game_idx;
}

void ServerGame::OnAct(elf::game::Base* base) {
  State state;
  Reply reply;

  while (true) {
    int q_idx;
    auto sampler = reader_->getSamplerWithParity(&base->rng(), &q_idx);
    const Record* p = sampler.sample();
    if (p == nullptr) {
      continue;
    }
    state = p->request.state;
    reply = p->result.reply;
  }

  auto *client = base->client();
  auto binder = client->getBinder();
  auto funcs = binder.BindStateToFunctions({"train"}, &state);
  auto funcs2 = binder.BindStateToFunctions({"train"}, &reply);
  funcs.add(funcs2);

  client->sendWait({"train"}, &funcs);
}
