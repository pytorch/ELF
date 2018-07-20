/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "elf/comm/primitive.h"
#include "elf/concurrency/ConcurrentQueue.h"
#include "elf/concurrency/Counter.h"
#include "elf/utils/member_check.h"

#include "tree_search_node.h"
#include "tree_search_options.h"

/*
 * Use the following function of S
 * Copy Constructor: duplicate the state.
 * s.set_thread(int i). Set the thread idx.
 * bool s.forward(const A& a). Forward function that changes the current state
 * to the next state. Return false if the current state is terminal.
 * float s.reward(). Get a reward given the current state.
 * s.evaluate(). Evaluate the current state to get pi/V.
 * s.pi(): return vector<pair<A, float>> for the candidate actions and its prob.
 * s.value(): return a float for the value of current state.
 *
 */

namespace elf {
namespace ai {
namespace tree_search {

struct RunContext {
  int run_id;
  int idx;
  int num_rollout;
  int depth;

  RunContext(int run_id, int idx, int num_rollout)
      : run_id(run_id), idx(idx), num_rollout(num_rollout), depth(0) {}

  void incDepth() {
    depth++;
  }
};

template <typename State, typename Action>
class TreeSearchSingleThreadT {
 public:
  using Node = NodeT<State, Action>;
  using SearchTree = SearchTreeT<State, Action>;

  TreeSearchSingleThreadT(int thread_id, const TSOptions& options)
      : threadId_(thread_id), options_(options) {
    if (options_.verbose) {
      std::string log_file =
          options_.log_prefix + std::to_string(thread_id) + ".txt";
      output_.reset(new std::ofstream(log_file));
    }
  }

  void notifyReady(int num_rollout) {
    runInfoWhenStateReady_.push(num_rollout);
  }

  template <typename Actor>
  bool run(
      int run_id,
      const std::atomic_bool* stop_search,
      Actor& actor,
      SearchTree& search_tree) {
    int num_rollout;
    runInfoWhenStateReady_.pop(&num_rollout);

    Node* root = search_tree.getRootNode();
    if (root == nullptr || root->getStatePtr() == nullptr) {
      if (stop_search == nullptr || !stop_search->load()) {
        std::cout << "[" << threadId_ << "] root node is nullptr!" << std::endl;
      }
      return false;
    }

    _set_ostream(actor);

    if (output_ != nullptr) {
      *output_ << "[run=" << run_id << "] " << actor.info() << std::endl
               << std::flush;
    }

    for (int idx = 0;
         idx < num_rollout && (stop_search == nullptr || !stop_search->load());
         idx += options_.num_rollouts_per_batch) {
      // Start from the root and run one path
      batch_rollouts<Actor>(
          RunContext(run_id, idx, num_rollout), root, actor, search_tree);
    }

    if (output_ != nullptr) {
      *output_ << "[run=" << run_id << "] "
               << "Done" << std::endl
               << std::flush;
    }
    return true;
  }

 private:
  int threadId_;
  const TSOptions& options_;

  struct Traj {
    std::vector<std::pair<Node*, Action>> traj;
    Node* leaf;
  };

  // TODO: The weird variable name below needs to change (ssengupta@fb)
  elf::concurrency::ConcurrentQueue<int> runInfoWhenStateReady_;
  std::unique_ptr<std::ostream> output_;

  MEMBER_FUNC_CHECK(reward)
  template <
      typename Actor,
      std::enable_if_t<has_func_reward<Actor>::value>* U = nullptr>
  float get_reward(const Actor& actor, const Node* node) {
    return actor.reward(*node->getStatePtr(), node->getValue());
  }

  template <
      typename Actor,
      typename std::enable_if<!has_func_reward<Actor>::value>::type* U =
          nullptr>
  float get_reward(const Actor& actor, const Node* node) {
    (void)actor;
    return node->getValue();
  }

  MEMBER_FUNC_CHECK(set_ostream)
  template <
      typename Actor,
      typename std::enable_if<has_func_set_ostream<Actor>::value>::type* U =
          nullptr>
  void _set_ostream(Actor& actor) {
    actor.set_ostream(output_.get());
  }

  template <
      typename Actor,
      typename std::enable_if<!has_func_set_ostream<Actor>::value>::type* U =
          nullptr>
  void _set_ostream(Actor&) {}

  template <typename Actor>
  bool allocateState(
      const Node* node,
      const Action& action,
      Actor& actor,
      Node* next_node) {
    auto func = [&]() -> State* {
      State* state = new State(*node->getStatePtr());
      if (!actor.forward(*state, action)) {
        delete state;
        return nullptr;
      }
      return state;
    };

    return next_node->setStateIfUnset(func);
  }

  void printHelper(const RunContext& ctx, std::string str) {
    if (output_ != nullptr) {
      *output_ << "[run=" << ctx.run_id << "][iter=" << ctx.idx << "/"
               << ctx.num_rollout << "][depth=" << ctx.depth << "] " << str
               << std::endl;
    }
  }

  template <typename Actor>
  void batch_rollouts(
      const RunContext& ctx,
      Node* root,
      Actor& actor,
      SearchTree& search_tree) {
    // Start from the root and run one path
    std::vector<Traj> trajs;
    for (int j = 0; j < options_.num_rollouts_per_batch; ++j) {
      trajs.push_back(single_rollout<Actor>(ctx, root, actor, search_tree));
    }

    // Now we want to batch create nodes.
    std::vector<Node*> locked_leaves;
    std::vector<const State*> locked_states;

    std::unordered_map<Node*, std::pair<Traj*, int>> traj_counts;

    // For unlocked leaves, just let it go
    // Reason:
    //   1. Other threads lock it
    //   2. Duplicated leaf.
    for (Traj& traj : trajs) {
      if (traj.leaf->requestEvaluation()) {
        locked_leaves.push_back(traj.leaf);
        locked_states.push_back(traj.leaf->getStatePtr());
      }

      auto it = traj_counts.find(traj.leaf);
      if (it == traj_counts.end())
        traj_counts[traj.leaf] = std::make_pair(&traj, 1);
      else
        it->second.second++;
    }

    // Batch evaluate.
    std::vector<NodeResponseT<Action>> resps;
    actor.evaluate(locked_states, &resps);

    for (size_t j = 0; j < locked_leaves.size(); ++j) {
      // Now the node points to a recently created node.
      // Evaluate it and backpropagate.
      locked_leaves[j]->setEvaluation(resps[j]);
    }

    for (auto& traj_pair : traj_counts) {
      Node* leaf = traj_pair.first;
      Traj* traj = traj_pair.second.first;
      int count = traj_pair.second.second;

      leaf->waitEvaluation();
      float reward = get_reward(actor, leaf);
      // PRINT_TS("Reward: " << reward << " Start backprop");

      // Add reward back.
      for (const auto& p : traj->traj) {
        p.first->updateEdgeStats(
            p.second, reward, options_.virtual_loss * count);
      }
    }

    printHelper(ctx, "Done backprop");
  }

  template <typename Actor>
  Traj single_rollout(
      RunContext ctx,
      Node* root,
      Actor& actor,
      SearchTree& search_tree) {
    Node* node = root;

    Traj traj;
    while (node->isVisited()) {
      // If there is no move available, skip.
      Action action;
      bool has_move =
          node->findMove(options_.alg_opt, ctx.depth, &action, output_.get());
      if (!has_move) {
        printHelper(ctx, "No available action");
        break;
      }

      // PRINT_TS(" Action: " << action);

      // Add virtual loss if there is any.
      if (options_.virtual_loss > 0) {
        node->addVirtualLoss(action, options_.virtual_loss);
      }

      // Save trajectory.
      traj.traj.push_back(std::make_pair(node, action));
      NodeId next = node->followEdge(action, search_tree);
      // PRINT_TS(" Descent node id: " << next);

      assert(node->getStatePtr());

      // Note that next might be invalid, if there is not valid move.
      Node* next_node = search_tree[next];
      if (next_node == nullptr) {
        break;
      }

      printHelper(ctx, "Before forward");
      // PRINT_TS(" Before forward. ");

      // actor takes action with node's state. If this
      // action is valid, then next_node is set with the new state
      // Otherwise next_node's state is a nullptr
      if (!allocateState(node, action, actor, next_node)) {
        break;
      }

      printHelper(ctx, "After forward");
      // PRINT_TS(" After forward. ");
      node = next_node;

      // PRINT_TS(" Next node address: " << hex << node << dec);
      ctx.incDepth();
    }
    traj.leaf = node;
    return traj;
  }
};

// Mcts algorithm
template <typename State, typename Action, typename Actor>
class TreeSearchT {
 public:
  using Node = NodeT<State, Action>;
  using TreeSearchSingleThread = TreeSearchSingleThreadT<State, Action>;
  using SearchTree = SearchTreeT<State, Action>;
  using MCTSResult = MCTSResultT<Action>;

  TreeSearchT(const TSOptions& options, std::function<Actor*(int)> actor_gen)
      : options_(options), stopSearch_(false) {
    for (int i = 0; i < options.num_threads; ++i) {
      treeSearches_.emplace_back(new TreeSearchSingleThread(i, options_));
      actors_.emplace_back(actor_gen(i));
    }

    // cout << "#Thread: " << options.num_threads << endl;
    for (int i = 0; i < options.num_threads; ++i) {
      TreeSearchSingleThread* th = treeSearches_[i].get();
      threadPool_.emplace_back(std::thread{[i, this, th]() {
        int counter = 0;
        while (true) {
          th->run(
              counter,
              // &this->done_.flag(),
              &this->stopSearch_,
              *this->actors_[i],
              this->searchTree_);

          // if (this->done_.get()) {
          if (this->stopSearch_.load()) {
            break;
          }

          this->treeReady_.increment();
          counter++;
        }
        this->countStoppedThreads_.increment();
        // this->done_.notify();
      }});
    }
  }

  Actor& getActor(int i) {
    return *actors_[i];
  }

  const Actor& getActor(int i) const {
    return *actors_[i];
  }

  size_t getNumActors() const {
    return actors_.size();
  }

  std::string printTree() const {
    return searchTree_.printTree();
  }

  MCTSResult runPolicyOnly(const State& root_state) {
    if (actors_.empty() || treeSearches_.empty()) {
      throw std::range_error(
          "TreeSearch::runPolicyOnly works when there is at least one thread");
    }
    setRootNodeState(root_state);

    // Some hack here.
    Node* root = searchTree_.getRootNode();

    if (!root->isVisited()) {
      NodeResponseT<Action> resp;
      actors_[0]->evaluate(*root->getStatePtr(), &resp);
      root->setEvaluation(resp);
    }

    MCTSResult result;
    result.action_rank_method = MCTSResult::PRIOR;
    result.addActions(root->getStateActions());
    result.root_value = root->getValue();
    return result;
  }

  MCTSResult run(const State& root_state) {
    setRootNodeState(root_state);

    if (options_.root_epsilon > 0.0) {
      Node* root = searchTree_.getRootNode();
      root->enhanceExploration(
          options_.root_epsilon, options_.root_alpha, actors_[0]->rng());
    }

    notifySearches(options_.num_rollouts_per_thread);

    // Wait until all tree searches are done.
    treeReady_.waitUntilCount(threadPool_.size());
    treeReady_.reset();

    return chooseAction();
  }

  void treeAdvance(const Action& action) {
    searchTree_.treeAdvance(action);
  }

  void clear() {
    searchTree_.clear();
  }

  void stop() {
    stopSearch_ = true;

    notifySearches(0);

    countStoppedThreads_.waitUntilCount(threadPool_.size());

    for (auto& p : threadPool_) {
      p.join();
    }
  }

  ~TreeSearchT() {
    if (!stopSearch_.load()) {
      stop();
    }
  }

 private:
  // Multiple threads.
  std::vector<std::thread> threadPool_;
  std::vector<std::unique_ptr<TreeSearchSingleThread>> treeSearches_;
  std::vector<std::unique_ptr<Actor>> actors_;

  std::unique_ptr<std::ostream> output_;

  SearchTree searchTree_;

  TSOptions options_;
  std::atomic<bool> stopSearch_;
  // Notif done_;
  elf::concurrency::Counter<size_t> treeReady_;
  elf::concurrency::Counter<size_t> countStoppedThreads_;

  void notifySearches(int num_rollout) {
    for (size_t i = 0; i < treeSearches_.size(); ++i) {
      treeSearches_[i]->notifyReady(num_rollout);
    }
  }

  void setRootNodeState(const State& root_state) {
    Node* root = searchTree_.getRootNode();

    if (root == nullptr) {
      throw std::range_error("TreeSearch::root cannot be null!");
    }

    root->setStateIfUnset([&]() { return new State(root_state); });

    // Check hash code.
    if (!elf::ai::tree_search::StateTrait<State, Action>::equals(
            root_state, *root->getStatePtr())) {
      throw std::range_error(
          "TreeSearch::Root state is not the same as the input state");
    }
  }

  MCTSResult chooseAction() const {
    const Node* root = searchTree_.getRootNode();
    if (root == nullptr) {
      std::cout << "TreeSearch::root cannot be null!" << std::endl;
      throw std::range_error("TreeSearch::root cannot be null!");
    }

    // Pick the best solution.
    MCTSResult result;
    result.root_value = root->getValue();

    // MCTSResult result2;
    if (options_.pick_method == "strongest_prior") {
      result.action_rank_method = MCTSResult::PRIOR;
      result.addActions(root->getStateActions());
      // result2 = StrongestPrior(root->getStateActions());
    } else if (options_.pick_method == "most_visited") {
      result.action_rank_method = MCTSResult::MOST_VISITED;
      result.addActions(root->getStateActions());
      // result2 = MostVisited(root->getStateActions());

      // assert(result.max_score == result2.max_score);
      // assert(result.total_visits == result2.total_visits);
    } else if (options_.pick_method == "uniform_random") {
      result.action_rank_method = MCTSResult::UNIFORM_RANDOM;
      result.addActions(root->getStateActions());
      // result = UniformRandom(root->getStateActions());
    } else {
      throw std::range_error(
          "MCTS Pick method unknown! " + options_.pick_method);
    }

    return result;
    // return result2;
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
