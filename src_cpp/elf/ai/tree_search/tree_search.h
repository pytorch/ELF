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

struct MCTSRunOptions {
  int64_t msec_start_time = -1;
  int64_t msec_time_left = -1;
  int64_t byoyomi = -1;
  void reset() {
    msec_start_time = -1;
    msec_time_left = -1;
    byoyomi = -1;
  }
};

enum MCTSSignal {
  MCTS_CMD_INVALID = -1,
  MCTS_CMD_PAUSE,
  MCTS_CMD_RESUME,
  MCTS_CMD_STOP, 
  MCTS_CMD_CHANGE_ROOT,
  MCTS_CMD_CHANGE_ROOT_AND_RESUME,
};

enum MCTSReply { MCTS_REPLY = 0 };
enum MCTSTimeCtrl { MCTS_ONTIME = 0, MCTS_TIMEOUT };

inline std::ostream& operator<<(std::ostream& oo, const MCTSSignal& signal) {
  switch (signal) {
    case MCTS_CMD_INVALID:
      oo << "MCTS_CMD_INVALID";
      break;
    case MCTS_CMD_PAUSE:
      oo << "MCTS_CMD_PAUSE";
      break;
    case MCTS_CMD_RESUME:
      oo << "MCTS_CMD_RESUME";
      break;
    case MCTS_CMD_STOP:
      oo << "MCTS_CMD_STOP";
      break;
    case MCTS_CMD_CHANGE_ROOT:
      oo << "MCTS_CMD_CHANGE_ROOT";
      break;
    case MCTS_CMD_CHANGE_ROOT_AND_RESUME:
      oo << "MCTS_CMD_CHANGE_ROOT_AND_RESUME";
      break;
  }
  return oo;
}

using SignalQ = elf::concurrency::ConcurrentQueue<MCTSSignal>;
using ReplyQ = elf::concurrency::ConcurrentQueue<MCTSReply>;

struct MCTSThreadState {
  int thread_id = -1;
  bool done = false;
  int num_rollout_curr_root = 0;
  int num_rollout_since_last_resume = 0;
};

using StateQ = elf::concurrency::ConcurrentQueue<MCTSThreadState>;

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

template <typename State, typename Action, typename Info>
class TreeSearchSingleThreadT {
 public:
  using Node = NodeT<State, Action, Info>;
  using SearchTree = SearchTreeT<State, Action, Info>;
  using NodeResponse = NodeResponseT<Action, Info>;

  TreeSearchSingleThreadT(int thread_id, const TSOptions& options)
      : threadId_(thread_id), options_(options) {
    if (options_.verbose) {
      std::string log_file =
          options_.log_prefix + std::to_string(thread_id) + ".txt";
      output_.reset(new std::ofstream(log_file));
    }
  }

  void sendSignal(const MCTSSignal& signal) {
    input_q_.push(signal);
  }

  MCTSReply waitSignalReceived() {
    MCTSReply reply;
    reply_q_.pop(&reply);
    return reply;
  }

  template <typename Actor>
  bool run(Actor& actor, SearchTree& search_tree, StateQ& ctrl) {
    _set_ostream(actor);
    bool wait_state = true;
    int rollouts_curr_root = 0;
    int rollouts_since_last_resume = 0;

    Node* root = nullptr;

    while (true) {
      MCTSSignal signal = MCTS_CMD_INVALID;
      if (input_q_.pop(&signal, std::chrono::seconds(0))) {
        switch (signal) {
          case MCTS_CMD_STOP:
            return true;
          case MCTS_CMD_RESUME:
            rollouts_since_last_resume = 0;
            wait_state = false;
            break;
          case MCTS_CMD_PAUSE:
            wait_state = true;
            break;
          case MCTS_CMD_CHANGE_ROOT:
          case MCTS_CMD_CHANGE_ROOT_AND_RESUME:
            root = search_tree.getRootNode();
            rollouts_curr_root = 0;
            //std::cout << "[" << threadId_ << "] " 
            //  << "Wait node spent: " << static_cast<float>(usec_wait_node_spent_) / 1e3 << " msec" 
            //  << "Evaluation spent: " << static_cast<float>(usec_evaluation_) / 1e3 << " msec" << std::endl;
            usec_wait_node_spent_ = 0;
            usec_evaluation_ = 0;
            if (signal == MCTS_CMD_CHANGE_ROOT_AND_RESUME) {
              rollouts_since_last_resume = 0;
              wait_state = false;
            }
            break;
          default:
            break;
        }
        reply_q_.push(MCTS_REPLY);
      }

      if (wait_state || root == nullptr) {
        // std::cout << "[" << std::this_thread::get_id() << "] In wait state,
        // sleep for a while" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      // Start from the root and run one path
      int num_rollout = batch_rollouts<Actor>(
          RunContext(threadId_, rollouts_curr_root, options_.num_rollout_per_thread),
          root,
          actor,
          search_tree);

      if (num_rollout == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      // std::cout << "#rollout: " << num_rollout << std::endl;
      rollouts_curr_root += num_rollout;
      rollouts_since_last_resume += num_rollout;

      MCTSThreadState state;
      state.thread_id = threadId_;
      state.num_rollout_curr_root = rollouts_curr_root;
      state.num_rollout_since_last_resume = rollouts_since_last_resume;
      const int max_rollouts = 1e6;
      const int min_rollouts = 1e2;
      if (((options_.num_rollout_per_thread > 0 &&
          rollouts_curr_root >= options_.num_rollout_per_thread) ||
          rollouts_curr_root >= max_rollouts) && 
          rollouts_curr_root >= min_rollouts) {
        state.done = true;
        wait_state = true;
      }
      ctrl.push(state);
    }
  }

 private:
  int threadId_;
  const TSOptions& options_;
  uint64_t usec_wait_node_spent_ = 0;
  uint64_t usec_evaluation_ = 0;

  struct Traj {
    std::vector<std::pair<Node*, Action>> traj;
    Node* leaf;
  };

  struct TrajCount {
    std::unordered_map<Node*, std::pair<Traj*, int>> counts;
    void add(Traj *traj) {
      auto it = counts.find(traj->leaf);
      if (it == counts.end())
        counts[traj->leaf] = std::make_pair(traj, 1);
      else
        it->second.second++;
    }

    const std::pair<Traj*, int> &find(Node *leaf) const {
      auto it = counts.find(leaf);
      assert(it != counts.end());
      return it->second;
    }
  };

  // TODO: The weird variable name below needs to change (ssengupta@fb)
  SignalQ input_q_;
  ReplyQ reply_q_;
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
  int batch_rollouts(
      const RunContext& ctx,
      Node* root,
      Actor& actor,
      SearchTree& search_tree) {
    // Start from the root and run one path
    std::vector<Traj> trajs;
    for (int j = 0; j < options_.num_rollout_per_batch; ++j) {
      trajs.push_back(single_rollout<Actor>(ctx, root, actor, search_tree));
    }

    // Now we want to batch create nodes.
    std::vector<Node*> locked_leaves;
    std::vector<const State*> locked_states;
    TrajCount ours, others;

    // For unlocked leaves, just let it go
    // Reason:
    //   1. Other threads lock it
    //   2. Duplicated leaf.
    int num_real_rollout = 0;
    for (Traj& traj : trajs) {
      if (traj.leaf->requestEvaluation()) {
        locked_leaves.push_back(traj.leaf);
        locked_states.push_back(traj.leaf->getStatePtr());
        ours.add(&traj);
        num_real_rollout ++;
      } else {
        others.add(&traj);
      }
    }

    auto backprop = [&](const std::pair<Traj *, int> &p) {
      Node *leaf = p.first->leaf;
      int count = p.second;

      usec_wait_node_spent_ += leaf->waitEvaluation();
      float reward = get_reward(actor, leaf);
      
      // std::cout << leaf->getStatePtr()->showBoard() << std::endl;
      // std::cout << "value: " << reward << std::endl << std::endl;
      // PRINT_TS("Reward: " << reward << " Start backprop");

      // Add reward back.
      for (const auto& pp : p.first->traj) {
        pp.first->updateEdgeStats(
            pp.second, reward, options_.virtual_loss * count);
      }
    };

    auto remove_virtual_loss = [&](const std::pair<Traj *, int> &p) {
      int count = p.second;

      for (const auto& pp : p.first->traj) {
        pp.first->addVirtualLoss(
            pp.second, -options_.virtual_loss * count);
      }
    };

    auto on_success = [&](size_t idx, NodeResponse &&resp) {
      // Now the node points to a recently created node.
      // Evaluate it and backpropagate.
      Node *leaf = locked_leaves[idx];
      leaf->setEvaluation(std::move(resp));

      const auto& record = ours.find(leaf);
      backprop(record);
    };

    // Batch evaluate.
    auto start = elf_utils::usec_since_epoch_from_now();
    actor.evaluate(locked_states, on_success);
    usec_evaluation_ += elf_utils::usec_since_epoch_from_now() - start;

    for (const auto &p : others.counts) {
      remove_virtual_loss(p.second);
    } 
    printHelper(ctx, "Done backprop");
    return num_real_rollout;
  }

  template <typename Actor>
  Traj single_rollout(
      RunContext ctx,
      Node* node,
      Actor& actor,
      SearchTree& search_tree) {
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
      NodeId next =
          node->followEdgeCreateIfNull(action, search_tree.getStorage());
      // PRINT_TS(" Descent node id: " << next);

      assert(node->getStatePtr());

      // Note that next might be invalid, if there is not valid move.
      Node* next_node = search_tree.getStorage()[next];
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
  using Info = typename Actor::Info;
  using TreeSearchSingleThread = TreeSearchSingleThreadT<State, Action, Info>;
  using Node = typename TreeSearchSingleThread::Node;
  using NodeResponse = typename TreeSearchSingleThread::NodeResponse;
  using SearchTree = typename TreeSearchSingleThread::SearchTree;
  using MCTSResult = MCTSResultT<Action>;

  TreeSearchT(const TSOptions& options, std::function<Actor*(int)> actor_gen)
      : options_(options) {
    for (int i = 0; i < options.num_thread; ++i) {
      treeSearches_.emplace_back(new TreeSearchSingleThread(i, options_));
      actors_.emplace_back(actor_gen(i));
    }

    // cout << "#Thread: " << options.num_threads << endl;
    for (int i = 0; i < options.num_thread; ++i) {
      TreeSearchSingleThread* th = treeSearches_[i].get();
      threadPool_.emplace_back(std::thread{[i, this, th]() {
        th->run(*actors_[i], searchTree_, ctrl_q_);
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

  SearchTree& getSearchTree() {
    return searchTree_;
  }

  MCTSResult runPolicyOnly() {
    if (actors_.empty() || treeSearches_.empty()) {
      throw std::range_error(
          "TreeSearch::runPolicyOnly works when there is at least one thread");
    }
    // Some hack here.
    Node* root = searchTree_.getRootNode();

    if (!root->isVisited()) {
      NodeResponse resp;
      actors_[0]->evaluate(*root->getStatePtr(), &resp);
      root->setEvaluation(std::move(resp));
    }

    return root->chooseAction(MCTSResult::PRIOR);
  }

  MCTSResult run(const MCTSRunOptions& run_options) {
    if (options_.root_epsilon > 0.0) {
      sendSearchSignal(MCTS_CMD_PAUSE);
      Node* root = searchTree_.getRootNode();
      root->getStateActionsMutable().enhanceExploration(
          options_.root_epsilon, options_.root_alpha, actors_[0]->rng());
    }
    sendSearchSignal(MCTS_CMD_CHANGE_ROOT_AND_RESUME);
    searchTree_.deleteOldRoot();

    std::vector<std::pair<int, int>> num_rollouts(threadPool_.size());
    size_t num_done = 0;
    uint64_t overhead = 0;
    while (true) {
      MCTSThreadState state;
      ctrl_q_.pop(&state);

      num_rollouts[state.thread_id] =
        std::make_pair(state.num_rollout_curr_root, state.num_rollout_since_last_resume);

      if (state.done) {
        num_done++;
        if (num_done == threadPool_.size()) {
          break;
        }
      }

      uint64_t now = elf_utils::msec_since_epoch_from_now();
      uint64_t dt = now - run_options.msec_start_time;
      if (overhead == 0) overhead = dt;
      if (timeCtrl(dt, overhead, run_options) == MCTS_TIMEOUT) {
        Node* root = searchTree_.getRootNode();
        if (root->isVisited()) {
          int count_curr_root = 0, count_since_last_resume = 0;
          for (const auto& p : num_rollouts) {
            count_curr_root += p.first;
            count_since_last_resume += p.second;
          }
          std::cout << "MCTS time spent: " << static_cast<float>(dt) / 1000.0
                    << "sec, #rollouts:, curr_root: " << count_curr_root
                    << ", since_resume: " << count_since_last_resume << std::endl;
          break;
        }
      }
    }
    return chooseAction();
  }

  MCTSTimeCtrl timeCtrl(uint64_t time_msec, uint64_t overhead, const MCTSRunOptions& run_options) {
    if (options_.time_sec_allowed_per_move < 0)
      return MCTS_ONTIME;
    uint64_t allowed_time = 0;
    uint64_t time_msec_per_move = (uint64_t)options_.time_sec_allowed_per_move * 1000;
    if (run_options.byoyomi == 1) {
      // respect last byoyomi
      allowed_time =  time_msec_per_move;
    } else {
      if (overhead * 3 > time_msec_per_move) {
        // too much overhead, willing to spend more time
        if (run_options.byoyomi == 0) {
          // add back overhead
          allowed_time = time_msec_per_move + overhead;
        } else {
          // use one byoyomi period
          allowed_time = time_msec_per_move * 2;
        }
      } else {
        allowed_time = time_msec_per_move;
      }
    }
    if (time_msec >= allowed_time)
      return MCTS_TIMEOUT;
    else
      return MCTS_ONTIME;
  }

  void stop() {
    if (threadPool_.empty())
      return;

    sendSearchSignal(MCTS_CMD_STOP);
    for (auto& p : threadPool_) {
      p.join();
    }
  }

  ~TreeSearchT() {
    stop();
  }

 private:
  // Multiple threads.
  std::vector<std::thread> threadPool_;
  std::vector<std::unique_ptr<TreeSearchSingleThread>> treeSearches_;
  std::vector<std::unique_ptr<Actor>> actors_;

  std::unique_ptr<std::ostream> output_;

  SearchTree searchTree_;
  StateQ ctrl_q_;
  TSOptions options_;

  void sendSearchSignal(const MCTSSignal& signal) {
    // std::cout << "Sending signal: " << signal << std::endl;
    for (size_t i = 0; i < treeSearches_.size(); ++i) {
      treeSearches_[i]->sendSignal(signal);
    }
    for (size_t i = 0; i < treeSearches_.size(); ++i) {
      treeSearches_[i]->waitSignalReceived();
    }
    // std::cout << "Finish sending signal: " << signal << std::endl;
  }

  MCTSResult chooseAction() {
    const Node* root = searchTree_.getRootNode();
    if (root == nullptr) {
      std::cout << "TreeSearch::root cannot be null!" << std::endl;
      throw std::range_error("TreeSearch::root cannot be null!");
    }

    // MCTSResult result2;
    if (options_.pick_method == "strongest_prior") {
      return root->chooseAction(MCTSResult::PRIOR);
    } else if (options_.pick_method == "most_visited") {
      return root->chooseAction(MCTSResult::MOST_VISITED);
      // result2 = MostVisited(root->getStateActions());

      // assert(result.max_score == result2.max_score);
      // assert(result.total_visits == result2.total_visits);
    } else if (options_.pick_method == "uniform_random") {
      return root->chooseAction(MCTSResult::UNIFORM_RANDOM);
      // result = UniformRandom(root->getStateActions());
    } else {
      throw std::range_error(
          "MCTS Pick method unknown! " + options_.pick_method);
    }
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
