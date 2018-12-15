/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "extractor.h"
#include "elf/comm/base.h"

namespace elf {
 
class Binder {
 public:
  using RetrieverFunc = std::function<const std::vector<std::string> * (const std::string &)>;

  Binder(const Extractor &e, RetrieverFunc retriever) 
    : extractor_(e), retriever_(retriever) { 
    assert(retriever_ != nullptr);
  }

  template <typename S>
  FuncsWithState BindStateToFunctions(
      const std::vector<std::string>& smem_names,
      S* s) {
    FuncsWithState funcsWithState;

    std::set<std::string> dup;

    for (const auto& name : smem_names) {
      const std::vector<std::string>* keys = retriever_(name);

      if (keys == nullptr) {
        continue;
      }

      for (const auto& key : *keys) {
        if (dup.find(key) != dup.end()) {
          continue;
        }

        const FuncMapBase* funcs = extractor_.getFunctions(key);

        if (funcs == nullptr) {
          continue;
        }

        if (funcsWithState.state_to_mem_funcs.addFunction(
              key, funcs->BindStateToStateToMemFunc(*s))) {
          // LOG(INFO) << "GetPackage: key: " << key << "Add s2m "
          //           << std:: endl;
        }

        if (funcsWithState.mem_to_state_funcs.addFunction(
              key, funcs->BindStateToMemToStateFunc(*s))) {
          // LOG(INFO) << "GetPackage: key: " << key << "Add m2s "
          //           << std::endl;
        }
        dup.insert(key);
      }
    }
    return funcsWithState;
  }

  template <typename S>
  std::vector<FuncsWithState> BindStateToFunctions(
      const std::vector<std::string>& smem_names,
      const std::vector<S*>& batch_s) {
    std::vector<FuncsWithState> batchFuncsWithState(batch_s.size());

    std::set<std::string> dup;

    for (const auto& name : smem_names) {
      const std::vector<std::string>* keys = retriever_(name);

      if (keys == nullptr) {
        continue;
      }

      for (const auto& key : *keys) {
        if (dup.find(key) != dup.end()) {
          continue;
        }

        const FuncMapBase* funcs = extractor_.getFunctions(key);

        if (funcs == nullptr) {
          continue;
        }

        for (size_t i = 0; i < batch_s.size(); ++i) {
          auto& funcsWithState = batchFuncsWithState[i];
          S* s = batch_s[i];

          if (funcsWithState.state_to_mem_funcs.addFunction(
                key, funcs->BindStateToStateToMemFunc(*s))) {
            // LOG(INFO) << "GetPackage: key: " << key << "Add s2m "
            //           << std:: endl;
          }

          if (funcsWithState.mem_to_state_funcs.addFunction(
                key, funcs->BindStateToMemToStateFunc(*s))) {
            // LOG(INFO) << "GetPackage: key: " << key << "Add m2s "
            //           << std::endl;
          }
        }
        dup.insert(key);
      }
    }
    return batchFuncsWithState;
  }

 private:
  const Extractor &extractor_;
  RetrieverFunc retriever_ = nullptr;
};

class GameClientInterface {
 public:
  // For Game side.
  virtual void start() = 0;
  virtual void End() = 0;
  virtual bool DoStopGames() = 0;

  // TODO: This function should go away (ssengupta@fb)
  virtual bool checkPrepareToStop() = 0;

  virtual Binder getBinder() const = 0;

  virtual comm::ReplyStatus sendWait(
      const std::vector<std::string>& targets,
      FuncsWithState* funcs) = 0;

  virtual comm::ReplyStatus sendBatchWait(
      const std::vector<std::string>& targets,
      const std::vector<FuncsWithState*>& funcs) = 0;

  virtual comm::ReplyStatus sendBatchesWait(
      const std::vector<std::string>& targets, 
      const std::vector<std::vector<FuncsWithState*>>& funcs,
      const std::vector<comm::SuccessCallback>& callbacks) = 0;

  template <typename S>
  bool sendWait(const std::string &target, S &s) {
    auto binder = getBinder();
    FuncsWithState funcs = binder.BindStateToFunctions({target}, &s);
    return sendWait({target}, &funcs) == comm::SUCCESS;
  }
};

}  // namespace elf
