/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <iostream>
#include <string>
#include <vector>

#include "elf/base/context.h"
#include "elf/utils/utils.h"

namespace elf {
namespace ai {

template <typename S, typename A>
class AI_T {
 public:
  using Action = A;
  using State = S;
  using ActionCallback = std::function<void (size_t, const Action &)>;

  struct BatchCtrl {
    ActionCallback action_cb = nullptr;
    size_t sub_batchsize = 0;
    bool hasBatchCtrl() const { 
      return action_cb != nullptr && sub_batchsize > 0; 
    }

    void apply(size_t offset, const std::vector<const A*> &replies) const {
      assert(action_cb != nullptr);
      assert(replies.size() <= sub_batchsize);
      for (size_t i = 0; i < replies.size(); ++i) { 
        action_cb(offset + i, *replies[i]);
      }
    }
  };

  AI_T() {}

  void setID(int id) {
    id_ = id;
    onSetID();
    // LOG(INFO) << "setID: " << id << std::endl;
  }

  int getID() const {
    return id_;
  }

  // Given the current state, perform action and send the action to _a;
  // Return false if this procedure fails.
  virtual bool act(const S&, A*) {
    return true;
  }

  virtual bool act_batch(
      const std::vector<const S*>& /*batch_s*/,
      const std::vector<A*>& /*batch_a*/,
      const BatchCtrl &) {
    return true;
  }

  bool act_batch(
      const std::vector<const S*>& batch_s,
      const std::vector<A*>& batch_a) {
    return act_batch(batch_s, batch_a, BatchCtrl());
  }

  // End the game
  virtual bool endGame(const S&) {
    return true;
  }

  virtual ~AI_T() {}

 protected:
  virtual void onSetID() {}

 private:
  int id_ = -1;
};

template <typename S, typename A>
class AIClientT : public AI_T<S, A> {
 public:
  using Action = A;
  using State = S;
  using BatchCtrl = typename AI_T<S, A>::BatchCtrl;

  AIClientT(elf::GameClientInterface* client, const std::vector<std::string>& targets)
      : client_(client), targets_(targets) {}

  // Given the current state, perform action and send the action to _a;
  // Return false if this procedure fails.
  bool act(const S& s, A* a) override {
    auto binder = client_->getBinder();
    elf::FuncsWithState funcs_s = binder.BindStateToFunctions(targets_, &s);
    elf::FuncsWithState funcs_a = binder.BindStateToFunctions(targets_, a);
    funcs_s.add(funcs_a);

    // return client_->sendWait(targets_, &funcs);
    comm::ReplyStatus status = client_->sendWait(targets_, &funcs_s);
    return status == comm::ReplyStatus::SUCCESS;
  }

  bool act_batch(
      const std::vector<const S*>& batch_s,
      const std::vector<A*>& batch_a, 
      const BatchCtrl &batch_ctrl) override {
    assert(batch_s.size() == batch_a.size());
    auto binder = client_->getBinder();

    std::vector<elf::FuncsWithState> funcs_s =
        binder.BindStateToFunctions(targets_, batch_s);
    std::vector<elf::FuncsWithState> funcs_a =
        binder.BindStateToFunctions(targets_, batch_a);

    // return client_->sendWait(targets_, &funcs);
    comm::ReplyStatus status;
    if (batch_ctrl.hasBatchCtrl()) {
      size_t num_subbatch = (batch_s.size() + batch_ctrl.sub_batchsize - 1) / batch_ctrl.sub_batchsize;

      std::vector<std::vector<elf::FuncsWithState*>> ptr_funcs_s(num_subbatch);
      std::vector<std::vector<const A*>> ptr_a(num_subbatch);
      std::vector<comm::SuccessCallback> callbacks(num_subbatch);

      size_t i = 0;
      for (size_t j = 0; j < num_subbatch; ++j) {
        callbacks[j] = [&, i, j]() {
          batch_ctrl.apply(i, ptr_a[j]);
        };

        for (size_t k = 0; k < batch_ctrl.sub_batchsize; ++k) {
          funcs_s[i].add(funcs_a[i]);
          
          ptr_funcs_s[j].push_back(&funcs_s[i]);
          ptr_a[j].push_back(batch_a[i]);
          ++i;
          if (i == batch_s.size()) break;
        }

        if (i == batch_s.size()) break;
      }

      status = client_->sendBatchesWait(targets_, ptr_funcs_s, callbacks);
    } else {
      std::vector<elf::FuncsWithState*> ptr_funcs_s;
      for (size_t i = 0; i < funcs_s.size(); ++i) {
        funcs_s[i].add(funcs_a[i]);
        ptr_funcs_s.push_back(&funcs_s[i]);
      }
      status = client_->sendBatchWait(targets_, ptr_funcs_s);
    }
    return status == comm::ReplyStatus::SUCCESS;
  }

 private:
  elf::GameClientInterface* client_;
  std::vector<std::string> targets_;
};

} // namespace ai
} // namespace elf
