/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "client_manager.h"

ClientInfo::State::State(const ClientManager& mgr) : mgr_(mgr) {
  last_state_update_ = mgr_.getCurrTimeStamp();
}

bool ClientInfo::State::CompareGame(const ModelPair& p) const {
  std::lock_guard<std::mutex> lock(mutex_);
  // cout << "CompareGame: p: " << p.black_ver << "/" << p.white_ver
  //      << ", last_state: " << last_state_.black << "/" <<
  //      last_state_.white << endl;
  return last_state_.black == p.black_ver && last_state_.white == p.white_ver;
}

bool ClientInfo::State::StateUpdate(const ThreadState& ts) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (last_state_ != ts) {
    last_state_ = ts;
    last_state_update_ = mgr_.getCurrTimeStamp();
    return true;
  } else {
    return false;
  }
}

ClientInfo::ClientInfo(
    const ClientManager& mgr,
    const std::string& id,
    int num_threads,
    int max_delay_sec)
    : mgr_(mgr), identity_(id), max_delay_sec_(max_delay_sec), seq_(0) {
  for (int i = 0; i < num_threads; ++i) {
    threads_.emplace_back(new State(mgr_));
  }
  last_update_ = mgr_.getCurrTimeStamp();
}

void ClientInfo::stateUpdate(const ThreadState& ts) {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(ts.thread_id >= 0 && ts.thread_id < (int)threads_.size());
  if (threads_[ts.thread_id]->StateUpdate(ts)) {
    last_update_ = mgr_.getCurrTimeStamp();
  }
}

ClientInfo::ClientChange ClientInfo::updateActive() {
  std::lock_guard<std::mutex> lock(mutex_);
  bool curr_active = (mgr_.getCurrTimeStamp() - last_update_ < max_delay_sec_);

  if (active_) {
    if (!curr_active) {
      active_ = false;
      return ALIVE2DEAD;
    } else {
      return ALIVE;
    }
  } else {
    if (curr_active) {
      active_ = true;
      return DEAD2ALIVE;
    } else {
      return DEAD;
    }
  }
}
