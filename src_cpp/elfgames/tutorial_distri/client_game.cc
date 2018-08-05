/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "client_game.h"

////////////////// GoGame /////////////////////
ClientGame::ClientGame(
    int game_idx,
    const elf::Options& options,
    ThreadedDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      options_(options) {}

bool ClientGame::OnReceive(const MsgRequest& request, RestartReply* reply) {
  if (*reply == RestartReply::UPDATE_COMPLETE)
    return false;

  bool is_waiting = request.vers.wait();
  bool is_prev_waiting = _state_ext.currRequest().vers.wait();

  if (options_.common.base.verbose && !(is_waiting && is_prev_waiting)) {
    logger_->debug(
        "Receive request: {}, old: {}",
        (!is_waiting ? request.info() : "[wait]"),
        (!is_prev_waiting ? _state_ext.currRequest().info() : "[wait]"));
  }

  bool same_vers = (request.vers == _state_ext.currRequest().vers);
  bool same_player_swap =
      (request.client_ctrl.player_swap ==
       _state_ext.currRequest().client_ctrl.player_swap);

  bool async = request.client_ctrl.async;

  bool no_restart =
      (same_vers || async) && same_player_swap && !is_prev_waiting;

  // Then we need to reset everything.
  _state_ext.setRequest(request);

  if (is_waiting) {
    *reply = RestartReply::ONLY_WAIT;
    return false;
  } else {
    if (!no_restart) {
      restart();
      *reply = RestartReply::UPDATE_MODEL;
      return true;
    } else {
      if (!async)
        *reply = RestartReply::UPDATE_REQUEST_ONLY;
      else {
        setAsync();
        if (same_vers)
          *reply = RestartReply::UPDATE_REQUEST_ONLY;
        else
          *reply = RestartReply::UPDATE_MODEL_ASYNC;
      }
      return false;
    }
  }
}

void ClientGame::OnAct(elf::game::Base* base) {
  elf::GameClient* client = base->ctx().client;
  base_ = base;

  if (_online_counter % 5 == 0) {
    using std::placeholders::_1;
    using std::placeholders::_2;
    auto f = std::bind(&GoGameSelfPlay::OnReceive, this, _1, _2);

    do {
      dispatcher_->checkMessage(_state_ext.currRequest().vers.wait(), f);
    } while (_state_ext.currRequest().vers.wait());
  }
  _online_counter++;
}
