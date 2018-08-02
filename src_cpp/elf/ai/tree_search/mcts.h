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
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "elf/ai/ai.h"
#include "elf/utils/member_check.h"
#include "elf/utils/utils.h"

#include "tree_search.h"

namespace elf {
namespace ai {
namespace tree_search {

template <typename Actor>
class MCTSAI_T : public AI_T<typename Actor::State, typename Actor::Action> {
 public:
  using State = typename Actor::State;
  using Action = typename Actor::Action;

  using AI = AI_T<typename Actor::State, typename Actor::Action>;

  using MCTSAI = MCTSAI_T<Actor>;
  using TreeSearch = elf::ai::tree_search::TreeSearchT<State, Action, Actor>;
  using MCTSResult = elf::ai::tree_search::MCTSResultT<Action>;

  MCTSAI_T(
      const elf::ai::tree_search::TSOptions& options,
      std::function<Actor*(int)> gen)
      : options_(options) {
    ts_.reset(new TreeSearch(options_, gen));
  }

  const elf::ai::tree_search::TSOptions& options() const {
    return options_;
  }

  TreeSearch* getEngine() {
    return ts_.get();
  }

  void setTimeLimit(int64_t msec_start_ts, int64_t msec_time_left, int64_t byoyomi) {
    if (msec_start_ts > 0) {
      run_options_.msec_start_time = msec_start_ts;
    } else {
      run_options_.msec_start_time = elf_utils::msec_since_epoch_from_now();
    }

    run_options_.msec_time_left = msec_time_left;
    run_options_.byoyomi = byoyomi;
  }

  bool act(const State& s, Action* a) override {
    //auto now = elf_utils::msec_since_epoch_from_now();
    if (run_options_.msec_start_time > 0) {
      //std::cout << "Before MCTS Overhead: "
      //          << now - run_options_.msec_start_time << " ms" << std::endl;
    }

    align_state(s);

    if (options_.verbose_time) {
      elf_utils::MyClock clock;
      clock.restart();

      lastResult_ = ts_->run(run_options_);

      clock.record("MCTS");
      std::cout << "[" << this->getID()
                << "] MCTSAI Result: " << lastResult_.info()
                << " Action:" << lastResult_.best_action << std::endl;
      std::cout << clock.summary() << std::endl;
    } else {
      lastResult_ = ts_->run(run_options_);
    }

    *a = lastResult_.best_action;
    run_options_.reset();
    return true;
  }

  bool actPolicyOnly(const State& s, Action* a) {
    align_state(s);
    lastResult_ = ts_->runPolicyOnly();

    *a = lastResult_.best_action;
    return true;
  }

  bool endGame(const State&) override {
    return true;
  }

  const MCTSResult& getLastResult() const {
    return lastResult_;
  }

  std::string getCurrentTree() const {
    std::stringstream ss;
    ss << options_.info(true) << std::endl;
    ss << elf::ai::tree_search::ActorTrait<Actor>::to_string(ts_->getActor(0))
       << std::endl;
    ss << ts_->getSearchTree().printTree() << std::endl;
    ss << "Last choice: " << lastResult_.info() << std::endl;
    return ss.str();
  }

  void align_state(const State& s) {
      auto& st = ts_->getSearchTree();

      if (!options_.persistent_tree) {
          st.resetTree(s);
      } else {
          const auto* root = st.getRootNode();
          if (root == nullptr) {
              //std::cout << "root nullptr, reseting Tree" << std::endl;
              st.resetTree(s);
          } else {
              std::vector<Action> recent_moves;
              bool move_valid =
              elf::ai::tree_search::StateTrait<State, Action>::moves_since(
                  s, *root->getStatePtr(), &recent_moves);
                  if (move_valid) {
                      //std::cout << "Applying recent moves: #moves: " << recent_moves.size()
                      //          << std::endl;
                      st.treeAdvance(recent_moves, s);
                  } else {
                      //std::cout << "Recent move invalid, resetting" << std::endl;
                      st.resetTree(s);
                  }
              }
          }
      }

  /*
  MEMBER_FUNC_CHECK(restart)
  template <typename Actor_ = Actor, typename
  enable_if<has_func_restart<Actor_>::value>::type *U = nullptr>
  bool endGame() override {
      for (size_t i = 0; i < ts_->size(); ++i) {
          ts_->actor(i).restart();
      }
      return true;
  }
  */

 protected:
  void onSetID() override {
    for (size_t i = 0; i < ts_->getNumActors(); ++i) {
      ts_->getActor(i).setID(this->getID());
    }
  }

 private:
  elf::ai::tree_search::TSOptions options_;
  std::unique_ptr<TreeSearch> ts_;
  size_t nextMoveNumber_ = 0;
  MCTSResult lastResult_;

  MCTSRunOptions run_options_;
};

} // namespace tree_search
} // namespace ai
} // namespace elf
