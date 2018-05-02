/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "tree_search_base.h"
#include "tree_search_options.h"

namespace elf {
namespace ai {
namespace tree_search {

template <typename State, typename Action>
class SearchTreeT;

template <typename State>
class NodeBaseT {
 public:
  enum StateType { NODE_STATE_NULL = 0, NODE_STATE_INVALID, NODE_STATE_SET };

  NodeBaseT() : stateType_(NODE_STATE_NULL) {}

  const State* getStatePtr() const {
    return state_.get();
  }

  bool setStateIfUnset(std::function<State*()> func) {
    if (func == nullptr) {
      return false;
    }

    std::lock_guard<std::mutex> lock(lockState_);

    if (stateType_ == NODE_STATE_INVALID) {
      return false;
    }

    if (stateType_ == NODE_STATE_SET) {
      return true;
    }

#if 0
    if (s_state_ == NODE_INVALID) {
      return false;
    }

    if (s_state_ == NODE_SET) {
      return true;
    }
#endif

    state_.reset(func());

    if (state_ == nullptr) {
      stateType_ = NODE_STATE_INVALID;
      return false;
    } else {
      stateType_ = NODE_STATE_SET;
      return true;
    }
  }

 protected:
  std::mutex lockState_;
  std::unique_ptr<State> state_;
  // TODO Poor choice of variable name - think later (ssengupta@fb)
  StateType stateType_;
};

// Tree node.
template <typename State, typename Action>
class NodeT : public NodeBaseT<State> {
 public:
  using Node = NodeT<State, Action>;
  using SearchTree = SearchTreeT<State, Action>;

  enum VisitType {
    NODE_NOT_VISITED = 0,
    NODE_JUST_VISITED,
    NODE_ALREADY_VISITED
  };

  NodeT(float unsigned_parent_q)
      : visited_(false), numVisits_(0), unsignedParentQ_(unsigned_parent_q) {
    unsignedMeanQ_ = unsignedParentQ_;
  }

  NodeT(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  const std::unordered_map<Action, EdgeInfo>& getStateActions() const {
    return stateActions_;
  }

  int getNumVisits() const {
    return numVisits_;
  }

  float getValue() const {
    return V_;
  }

  float getMeanUnsignedQ() const {
    return unsignedMeanQ_;
  }

  bool isVisited() const {
    return visited_;
  }

  void enhanceExploration(float epsilon, float alpha, std::mt19937* rng) {
    // Note that this is not thread-safe.
    // It should be called once and only once for each node.
    if (epsilon == 0.0) {
      return;
    }

    std::gamma_distribution<> dis(alpha);

    // Draw distribution.
    std::vector<float> etas(stateActions_.size());
    float Z = 1e-10;
    for (size_t i = 0; i < stateActions_.size(); ++i) {
      etas[i] = dis(*rng);
      Z += etas[i];
    }

    int i = 0;
    for (auto& p : stateActions_) {
      p.second.prior_probability =
          (1 - epsilon) * p.second.prior_probability + epsilon * etas[i] / Z;
      i++;
    }
  }

  bool lockNodeForEvaluation() {
    if (visited_)
      return false;
    return lockNode_.try_lock();
  }

  void waitForEvaluation() {
    // Simple busy wait here.
    while (!visited_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  bool setNodeEvaluationAndUnlock(const NodeResponseT<Action>& resp) {
    if (visited_)
      return false;
    // Then we need to allocate sa_val_
    for (const std::pair<Action, float>& action_pair : resp.pi) {
      stateActions_.insert(
          std::make_pair(action_pair.first, EdgeInfo(action_pair.second)));
      lockStateActions_[action_pair.first].reset(new std::mutex());

      // Compute v here.
      // Node *child = alloc[res.first->second.next];
      // child->V_ = V_ + log(action_pair.second + 1e-6);
    }

    // value
    V_ = resp.value;
    flipQSign_ = resp.q_flip;

    // Once sa_ is allocated, its structure won't change.
    visited_ = true;
    lockNode_.unlock();
    return true;
  }

  template <typename ExpandFunc>
  VisitType expandIfNecessary(ExpandFunc func) {
    if (visited_) {
      return NODE_ALREADY_VISITED;
    }

    // Otherwise visit.
    std::lock_guard<std::mutex> lock(lockNode_);

    if (visited_) {
      return NODE_ALREADY_VISITED;
    }

    NodeResponseT<Action> resp;
    func(this, &resp);

    // Then we need to allocate sa_val_
    for (const std::pair<Action, float>& action_pair : resp.pi) {
      stateActions_.insert(
          std::make_pair(action_pair.first, EdgeInfo(action_pair.second)));
      lockStateActions_[action_pair.first].reset(new std::mutex());

      // Compute v here.
      // Node *child = alloc[res.first->second.next];
      // child->V_ = V_ + log(action_pair.second + 1e-6);
    }

    // value
    V_ = resp.value;
    flipQSign_ = resp.q_flip;

    // Once sa_ is allocated, its structure won't change.
    visited_ = true;
    return NODE_JUST_VISITED;
  }

  bool findMove(
      const SearchAlgoOptions& alg_opt,
      int node_depth,
      // const NodeDynInfo& node_info,
      Action* action,
      std::ostream* oo = nullptr) {
    std::lock_guard<std::mutex> lock(lockNode_);

    if (stateActions_.empty()) {
      return false;
    }

    if (alg_opt.unexplored_q_zero ||
        (alg_opt.root_unexplored_q_zero && node_depth == 0)) {
      unsignedMeanQ_ = 0.0;
    }

    BestAction best_action = UCT(alg_opt, oo);
    *action = best_action.action_with_max_score;
    unsignedMeanQ_ = (unsignedParentQ_ + best_action.total_unsigned_q) /
        (best_action.total_visits + 1);

    return true;
  }

  bool addVirtualLoss(const Action& action, float virtual_loss) {
    auto it = stateActions_.find(action);

    if (it == stateActions_.end()) {
      return false;
    }

    EdgeInfo& info = it->second;

    auto lock_it = lockStateActions_.find(action);
    assert(lock_it != lockStateActions_.end());
    std::lock_guard<std::mutex> lock(*(lock_it->second.get()));

    info.virtual_loss += virtual_loss;
    return true;
  }

  bool updateEdgeStats(const Action& action, float reward, float virtual_loss) {
    auto it = stateActions_.find(action);
    if (it == stateActions_.end()) {
      return false;
    }

    EdgeInfo& edge = it->second;

    numVisits_++;

    // Async modification (we probably need to add a locker in the future, or
    // not for speed).

    auto lock_it = lockStateActions_.find(action);
    assert(lock_it != lockStateActions_.end());
    std::lock_guard<std::mutex> lock(*(lock_it->second.get()));

    edge.reward += reward;
    edge.num_visits++;
    // Reduce virtual loss.
    edge.virtual_loss -= virtual_loss;
    return true;
  }

  NodeId followEdge(const Action& action, SearchTree& tree) {
    auto it = stateActions_.find(action);
    if (it == stateActions_.end()) {
      return InvalidNodeId;
    }

    EdgeInfo& edge = it->second;

    if (edge.child_node == InvalidNodeId) {
      auto lock_it = lockStateActions_.find(action);
      assert(lock_it != lockStateActions_.end());
      std::lock_guard<std::mutex> lock(*(lock_it->second.get()));

      // Need to check twice.
      if (edge.child_node == InvalidNodeId) {
        edge.child_node = tree.addNode(unsignedMeanQ_);
      }
    }
    return edge.child_node;
  }

 private:
  // for unit-test purpose only
  friend class NodeTest;

  std::mutex lockNode_;
  std::atomic<bool> visited_;
  std::unordered_map<Action, EdgeInfo> stateActions_;
  std::unordered_map<Action, std::unique_ptr<std::mutex>> lockStateActions_;

  std::atomic<int> numVisits_;
  float V_ = 0.0;
  float unsignedMeanQ_ = 0.0;

  // TODO Poor choice of variable name - fix later (ssengupta@fb)
  const float unsignedParentQ_;
  bool flipQSign_ = false;

  struct BestAction {
    Action action_with_max_score;
    float max_score;
    float total_unsigned_q;
    int total_visits;

    BestAction()
        : action_with_max_score(ActionTrait<Action>::default_value()),
          max_score(std::numeric_limits<float>::lowest()),
          total_unsigned_q(0),
          total_visits(0) {}

    void addAction(
        const Action& action,
        float score,
        float unsigned_q,
        bool first_visit) {
      if (score > max_score) {
        max_score = score;
        action_with_max_score = action;
      }

      if (!first_visit) {
        total_unsigned_q += unsigned_q;
        total_visits++;
      }
    }

    std::string info() const {
      std::stringstream ss;
      ss << " max_score: " << max_score << ", best_action: "
         << ActionTrait<Action>::to_string(action_with_max_score)
         << ", mean unsigned_q stats: "
         << (total_visits > 0 ? total_unsigned_q / total_visits : 0.0) << "/"
         << total_visits;
      return ss.str();
    }
  };

  // Algorithms.
  BestAction UCT(const SearchAlgoOptions& alg_opt, std::ostream* oo = nullptr)
      const {
    BestAction best_action;

    if (oo) {
      *oo << "uct prior = " << std::string(alg_opt.use_prior ? "True" : "False")
          << ", parent_cnt: " << (numVisits_.load() + 1) << std::endl;
    }

    for (const auto& action_pair : stateActions_) {
      const Action& action = action_pair.first;
      const EdgeInfo& edge = action_pair.second;

      // num_visits_ + 1 is sum of all visits to all other actions from
      // this node
      const int all_visits = numVisits_.load() + 1;
      auto prior_score = edge.getScore(flipQSign_, all_visits, unsignedMeanQ_);

      float score = alg_opt.use_prior
          ? (prior_score.prior_probability * alg_opt.c_puct + prior_score.q)
          : prior_score.q;

      best_action.addAction(
          action, score, prior_score.unsigned_q, prior_score.first_visit);

      if (oo) {
        *oo << "UCT [a=" << ActionTrait<Action>::to_string(action)
            << "][score=" << score << "] " << edge.info(true) << std::endl;
      }
    }
    if (oo) {
      *oo << "Get best action. uct prior = "
          << std::string(alg_opt.use_prior ? "True" : "False")
          << best_action.info() << std::endl;
    }
    return best_action;
  };
};

template <typename State, typename Action>
class SearchTreeT {
 public:
  using Node = NodeT<State, Action>;
  using SearchTree = SearchTreeT<State, Action>;

  SearchTreeT() {
    clear();
  }

  SearchTreeT(const SearchTree&) = delete;
  SearchTree& operator=(const SearchTree&) = delete;

  void clear() {
    allocatedNodes_.clear();
    allocatedNodeCount_ = 0;
    rootId_ = InvalidNodeId;
    allocateRoot();
  }

  void treeAdvance(const Action& action) {
    NodeId next_root = InvalidNodeId;
    Node* r = getRootNode();

    for (const auto& p : r->getStateActions()) {
      if (p.first == action) {
        next_root = p.second.child_node;
      } else {
        recursiveFree(p.second.child_node);
      }
    }

    // Free root.
    freeNode(rootId_);
    rootId_ = next_root;
    allocateRoot();
  }

  Node* getRootNode() {
    return (*this)[rootId_];
  }

  const Node* getRootNode() const {
    return (*this)[rootId_];
  }

  // Low level functions.
  NodeId addNode(float unsigned_parent_q) {
    std::lock_guard<std::mutex> lock(allocMutex_);
    allocatedNodes_[allocatedNodeCount_].reset(new Node(unsigned_parent_q));
    return allocatedNodeCount_++;
  }

  void freeNode(NodeId id) {
    allocatedNodes_.erase(id);
  }

  void recursiveFree(NodeId id) {
    if (id == InvalidNodeId) {
      return;
    }
    Node* root = (*this)[id];
    for (const auto& p : root->getStateActions()) {
      p.second.checkValid();
      recursiveFree(p.second.child_node);
    }
    freeNode(id);
  }

  Node* operator[](NodeId i) {
    std::lock_guard<std::mutex> lock(allocMutex_);
    return getNode(i);
  }

  const Node* operator[](NodeId i) const {
    std::lock_guard<std::mutex> lock(allocMutex_);
    return getNode(i);
  }

  std::string printTree() const {
    // [TODO]: Only called when no search is performed!
    return printTree(0, getRootNode());
  }

  std::string printTree(int indent, const Node* node) const {
    std::stringstream ss;
    std::string indent_str;
    for (int i = 0; i < indent; ++i) {
      indent_str += ' ';
    }

    int total_n = 0;

    for (const auto& p : node->getStateActions()) {
      if (p.second.num_visits > 0) {
        const Node* n = getNode(p.second.child_node);
        if (n->isVisited()) {
          ss << indent_str << ActionTrait<Action>::to_string(p.first) << " "
             << p.second.info();
          ss << ", V: " << n->getValue();
          assert(n->getStatePtr() != nullptr);
          std::string state_info =
              StateTrait<State, Action>::to_string(*n->getStatePtr());
          if (!state_info.empty()) {
            ss << ", " << state_info;
          }
          ss << ", unsigned_mean_q_: " << n->getMeanUnsignedQ() << std::endl;
          ss << printTree(indent + 2, n);
        }
        total_n += p.second.num_visits;
      } else {
        if (indent == 0) {
          ss << indent_str << ActionTrait<Action>::to_string(p.first) << " "
             << p.second.info() << std::endl;
        }
      }
    }
    if (indent == 0) {
      ss << indent_str << "- Total visit: " << total_n << std::endl;
      // Also print out entropy
      float entropy = 0.0;
      for (const auto& p : node->getStateActions()) {
        entropy -= p.second.prior_probability *
            log(p.second.prior_probability + 1e-10);
      }
      ss << indent_str << "- Prior Entropy: " << entropy << std::endl;
    }
    return ss.str();
  }

 private:
  // TODO: We might just allocate one chunk at a time.
  std::unordered_map<NodeId, std::unique_ptr<Node>> allocatedNodes_;
  NodeId allocatedNodeCount_;
  NodeId rootId_;
  mutable std::mutex allocMutex_;

  const Node* getNode(NodeId i) const {
    auto it = allocatedNodes_.find(i);
    if (it == allocatedNodes_.end()) {
      return nullptr;
    } else {
      return it->second.get();
    }
  }

  Node* getNode(NodeId i) {
    auto it = allocatedNodes_.find(i);
    if (it == allocatedNodes_.end()) {
      return nullptr;
    } else {
      return it->second.get();
    }
  }

  bool allocateRoot() {
    if (rootId_ == InvalidNodeId) {
      rootId_ = addNode(0.0);
      return true;
    }
    return false;
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
