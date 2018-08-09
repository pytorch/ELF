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
#include "elf/logging/IndexedLoggerFactory.h"
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
      : options_(options),
        logger_(
            elf::logging::getLogger("elf::ai::tree_search::MCTSAI_T-", "")) {
    ts_.reset(new TreeSearch(options_, gen));
  }

  const elf::ai::tree_search::TSOptions& options() const {
    return options_;
  }

  TreeSearch* getEngine() {
    return ts_.get();
  }

  bool act(const State& s, Action* a) override {
    align_state(s);

    if (options_.verbose_time) {
      elf_utils::MyClock clock;
      clock.restart();

      lastResult_ = ts_->run(s);

      clock.record("MCTS");
      logger_->info(
          "[{}] MCTSAI Result: {} Action: {}\n{}",
          this->getID(),
          lastResult_.info(),
          lastResult_.best_action,
          clock.summary());
    } else {
      lastResult_ = ts_->run(s);
    }

    *a = lastResult_.best_action;
    return true;
  }

  bool actPolicyOnly(const State& s, Action* a) {
    align_state(s);
    lastResult_ = ts_->runPolicyOnly(s);

    *a = lastResult_.best_action;
    return true;
  }

  bool endGame(const State&) override {
    resetTree();
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
    ss << ts_->printTree() << std::endl;
    ss << "Last choice: " << lastResult_.info() << std::endl;
    return ss.str();
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
  std::shared_ptr<spdlog::logger> logger_;

  void resetTree() {
    ts_->clear();
    nextMoveNumber_ = 0;
  }

  void align_state(const State& s) {
    if (!options_.persistent_tree) {
      resetTree();
    } else {
      advanceMoves(s);
    }
  }

  // Note that moves_since should have the following signature.
  //   bool moves_since(size_t *nextMoveNumber, vector<Action> *recent_moves)
  // It will compare the current nextMoveNumber to the move number in the
  // state, and return moves since the last move number in recent_moves
  // Once it is done, the move_number will be advanced to the most recent move
  // number.
  void advanceMoves(const State& s) {
    std::vector<Action> recent_moves;
    bool move_valid =
        elf::ai::tree_search::StateTrait<State, Action>::moves_since(
            s, &nextMoveNumber_, &recent_moves);
    if (move_valid) {
      for (const Action& prev_move : recent_moves) {
        ts_->treeAdvance(prev_move);
      }
    } else {
      resetTree();
    }
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
