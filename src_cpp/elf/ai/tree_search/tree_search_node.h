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
#include <list>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "elf/concurrency/ConcurrentQueue.h"
#include "tree_search_options.h"

namespace elf {
namespace ai {
namespace tree_search {

template <typename State, typename Action, typename Info>
class SearchTreeStorageT;

template <typename State>
class NodeBaseT {
 public:
  enum StateType { NODE_STATE_NULL = 0, NODE_STATE_INVALID, NODE_STATE_SET };

  NodeBaseT() {}

  // It will be called in a single thread after it is moved out of active trees. 
  void Init() {
    stateType_ = NODE_STATE_NULL;
    state_.reset();
  }

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

  virtual ~NodeBaseT() = default;

 protected:
  mutable std::mutex lockState_;
  std::unique_ptr<State> state_;
  // TODO Poor choice of variable name - think later (ssengupta@fb)
  StateType stateType_;
};

// Tree node.
template <typename State, typename Action, typename Info>
class NodeT : public NodeBaseT<State> {
 public:
  using NodeBase = NodeBaseT<State>;
  using Node = NodeT<State, Action, Info>;
  using NodeResponse = NodeResponseT<Action, Info>;
  using SearchTreeStorage = SearchTreeStorageT<State, Action, Info>;
  using MCTSRes = MCTSResultT<Action>;

  enum VisitType {
    NOT_VISITED = 0,
    EVAL_REQUESTED,
    VISITED,
  };

  NodeT() {}
  NodeT(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  void setId(NodeId id) { id_ = id; }

  // It will be called in a single thread after it is moved out of active trees. 
  // Once it is called, all the nodeIds will be returned (they are the next batch of free nodes). 
  std::list<NodeId> Init(NodeId parent, const Action& parent_a, float unsigned_parent_q) {
    NodeBase::Init();
    status_ = NOT_VISITED;
    numVisits_ = 0,
    unsignedParentQ_ = unsigned_parent_q;
    unsignedMeanQ_ = unsignedParentQ_;

    parent_ = parent;
    parent_a_ = parent_a;

    std::list<NodeId> nodes;
    for (const auto &p : stateActions_.pi) {
      if (p.second.child_node != InvalidNodeId) 
        nodes.push_back(p.second.child_node);
    }
    stateActions_.clear();

    return nodes;
  }

  const NodeResponse& getStateActions() const {
    return stateActions_;
  }

  NodeResponse& getStateActionsMutable() {
    return stateActions_;
  }

  MCTSRes chooseAction(typename MCTSRes::RankCriterion rc) const {
    std::lock_guard<std::mutex> lock(lockNode_);
    return MCTSRes(rc, stateActions_);
  }

  int getNumVisits() const {
    return numVisits_;
  }

  float getValue() const {
    return stateActions_.value;
  }

  float getMeanUnsignedQ() const {
    return unsignedMeanQ_;
  }

  VisitType status() const {
    return status_;
  }

  bool isVisited() const {
    return status_ == VISITED;
  }

  bool requestEvaluation() {
    if (status_ != NOT_VISITED)
      return false;

    std::lock_guard<std::mutex> lock(lockNode_);
    if (status_ != NOT_VISITED)
      return false;

    status_ = EVAL_REQUESTED;
    return true;
  }

  uint64_t waitEvaluation() {
    // Simple busy wait here.
    auto start = elf_utils::usec_since_epoch_from_now();
    while (status_ != VISITED) {
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    return elf_utils::usec_since_epoch_from_now() - start;
  }

  bool setEvaluation(NodeResponse&& resp) {
    if (status_ == VISITED)
      return false;

    std::lock_guard<std::mutex> lock(lockNode_);

    if (status_ == VISITED)
      return false;

    // Then we need to allocate sa_val_
    stateActions_ = std::move(resp);

    // Once sa_ is allocated, its structure won't change.
    status_ = VISITED;
    return true;
  }

  bool findMove(
      const SearchAlgoOptions& alg_opt,
      int node_depth,
      // const NodeDynInfo& node_info,
      Action* action,
      std::ostream* oo = nullptr) {
    if (status_ != VISITED)
      return false;

    std::lock_guard<std::mutex> lock(lockNode_);

    if (stateActions_.pi.empty()) {
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
    if (status_ != VISITED)
      return false;

    auto it = stateActions_.pi.find(action);
    if (it == stateActions_.pi.end()) {
      return false;
    }

    EdgeInfo& info = it->second;
    std::lock_guard<std::mutex> lockNode(lockNode_);

    info.virtual_loss += virtual_loss;
    return true;
  }

  bool updateEdgeStats(const Action& action, float reward, float virtual_loss) {
    if (status_ != VISITED)
      return false;

    auto it = stateActions_.pi.find(action);
    if (it == stateActions_.pi.end()) {
      return false;
    }

    EdgeInfo& edge = it->second;
    std::lock_guard<std::mutex> lockNode(lockNode_);

    numVisits_++;

    // Async modification (we probably need to add a locker in the future, or
    // not for speed).
    //
    edge.reward += reward;
    edge.num_visits++;
    // Reduce virtual loss.
    edge.virtual_loss -= virtual_loss;
    return true;
  }

  NodeId followEdgeCreateIfNull(const Action& action, SearchTreeStorage& tree) {
    if (status_ != VISITED)
      return InvalidNodeId;

    auto it = stateActions_.pi.find(action);
    if (it == stateActions_.pi.end()) {
      return InvalidNodeId;
    }

    EdgeInfo& edge = it->second;

    if (edge.child_node == InvalidNodeId) {
      std::lock_guard<std::mutex> lockNode(lockNode_);
      // Need to check twice.
      if (edge.child_node == InvalidNodeId) {
        edge.child_node = tree.allocateNode(id_, action, unsignedMeanQ_);
      }
    }
    return edge.child_node;
  }

  void detachFromParent(SearchTreeStorage& tree) {
    // Only call if no mcts thread has used the parent anymore. 
    if (parent_ == InvalidNodeId) return;
    Node *r = tree[parent_];
    
    auto &pi = r->stateActions_.pi;
    auto it = pi.find(parent_a_);
    assert(it != pi.end());
    it->second.child_node = InvalidNodeId;
  }

 private:
  // for unit-test purpose only
  friend class NodeTest;

  std::atomic<VisitType> status_;
  mutable std::mutex lockNode_;
  NodeResponse stateActions_;

  std::atomic<int> numVisits_;
  float unsignedMeanQ_ = 0.0;

  // TODO Poor choice of variable name - fix later (ssengupta@fb)
  float unsignedParentQ_;

  NodeId id_;

  NodeId parent_ = InvalidNodeId;
  Action parent_a_;

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
      *oo << "parent_cnt: " << (numVisits_.load() + 1) << std::endl;
    }

    for (const auto& action_pair : stateActions_.pi) {
      const Action& action = action_pair.first;
      const EdgeInfo& edge = action_pair.second;

      // num_visits_ + 1 is sum of all visits to all other actions from
      // this node
      const int all_visits = numVisits_.load() + 1;
      auto prior_score =
          edge.getScore(stateActions_.q_flip, all_visits, unsignedMeanQ_);

      float score = alg_opt.c_puct > 0
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
      *oo << "Get best action. " << best_action.info() << std::endl;
    }
    return best_action;
  };
};

template <typename State, typename Action, typename Info>
class SearchTreeStorageT {
 public:
  using Node = NodeT<State, Action, Info>;
  using SearchTreeStorage = SearchTreeStorageT<State, Action, Info>;

  SearchTreeStorageT(size_t max_num_node) 
    : numAllocated_(0), storage_(max_num_node)  {
     for (size_t i = 0; i < max_num_node; ++i) {
       storage_[i].setId(i);
       freeTreeRoots_.push_back(i);
     }
  }

  SearchTreeStorageT(const SearchTreeStorage&) = delete;
  SearchTreeStorage& operator=(const SearchTreeStorage&) = delete;

  // Low level functions.
  NodeId allocateNode(NodeId parent, const Action &parent_a, float unsigned_parent_q) {
    NodeId i = _alloc();

    Node *node = getNode(i);
    
    _free(node->Init(parent, parent_a, unsigned_parent_q));
    return i;
  }

  void releaseSubTree(NodeId id, NodeId except_node_id) {
    if (id == InvalidNodeId || id == except_node_id) {
      return;
    }

    (*this)[except_node_id]->detachFromParent(*this);

    std::lock_guard<std::mutex> lock(allocMutex_);
    freeTreeRoots_.push_back(id);
  }

  std::string info() const {
    std::stringstream ss;
    ss << "#Allocated: " << numAllocated_ << ", #Freed: " << numFreed_;
    return ss.str();
  }

  Node* operator[](NodeId i) {
    return getNode(i);
  }

  const Node* operator[](NodeId i) const {
    return getNode(i);
  }

  std::string printTree(int indent, const Node* node) const {
    std::stringstream ss;
    std::string indent_str;
    for (int i = 0; i < indent; ++i) {
      indent_str += ' ';
    }

    int total_n = 0;

    for (const auto& p : node->getStateActions().pi) {
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
      for (const auto& p : node->getStateActions().pi) {
        entropy -= p.second.prior_probability *
            log(p.second.prior_probability + 1e-10);
      }
      ss << indent_str << "- Prior Entropy: " << entropy << std::endl;
    }
    return ss.str();
  }

 private:
  std::atomic<int> numAllocated_;
  std::atomic<int> numFreed_;

  mutable std::mutex allocMutex_;

  // Preallocated storage. 
  std::vector<Node> storage_;

  // Free tree roots.
  std::list<NodeId> freeTreeRoots_;

  const Node* getNode(NodeId i) const {
    if (i == InvalidNodeId) return nullptr;
    else return &storage_[i];
  }

  Node* getNode(NodeId i) {
    if (i == InvalidNodeId) return nullptr;
    else return &storage_[i];
  }

  NodeId _alloc() {
    std::lock_guard<std::mutex> lock(allocMutex_);
    if (freeTreeRoots_.empty()) {
      throw std::runtime_error("Out of memory!!");
    }

    NodeId i = freeTreeRoots_.back();
    freeTreeRoots_.pop_back();
    return i;
  }

  void _free(std::list<NodeId> &&ids) {
    std::lock_guard<std::mutex> lock(allocMutex_);
    freeTreeRoots_.splice(freeTreeRoots_.end(), std::move(ids));
  }
};

template <typename State, typename Action, typename Info>
class SearchTreeT {
 public:
  using Node = NodeT<State, Action, Info>;
  using SearchTree = SearchTreeT<State, Action, Info>;
  using SearchTreeStorage = SearchTreeStorageT<State, Action, Info>;

  SearchTreeT() : 
    tree_(10000000),
    oldRootId_(InvalidNodeId), 
    rootId_(InvalidNodeId) {
  }

  SearchTreeT(const SearchTree&) = delete;
  SearchTree& operator=(const SearchTree&) = delete;

  SearchTreeStorage& getStorage() {
    return tree_;
  }

  void resetTree(const State& s) {
    NodeId root = tree_.allocateNode(InvalidNodeId, Action(), 0.0);
    tree_[root]->setStateIfUnset([&]() { return new State(s); });
    setNewRoot(root);
  }

  void treeAdvance(const std::vector<Action>& actions, const State& s) {
    // Here we assume that only one thread can change rootId_ (e.g., calling
    // setNewRoot).
    NodeId next_root = rootId_;

    Node* r = tree_[rootId_];
    assert(r != nullptr);

    for (const Action& action : actions) {
      // It will allocate new node if that node is null.
      // std::cout << "applying action " <<
      // ActionTrait<Action>::to_string(action) << std::endl;
      next_root = r->followEdgeCreateIfNull(action, tree_);
      if (next_root == InvalidNodeId) {

          next_root = tree_.allocateNode(InvalidNodeId, Action(), 0.0);
          r = tree_[next_root];
          break;
      }
      r = tree_[next_root];
    }

    r->setStateIfUnset([&]() { return new State(s); });

    // Check.
    if (!StateTrait<State, Action>::equals(s, *r->getStatePtr())) {
      std::cout << "TreeSearch::Root state is not the same as the input state"
                << std::endl;
      throw std::range_error(
          "TreeSearch::Root state is not the same as the input state");
    }

    setNewRoot(next_root);
  }

  Node* getRootNode() {
    std::lock_guard<std::mutex> lock(rootMutex_);
    return tree_[rootId_];
  }

  void setNewRoot(NodeId next_root) {
    std::lock_guard<std::mutex> lock(rootMutex_);
    // std::cout << "Setting new root proposal " << std::endl;
    if (oldRootId_ == InvalidNodeId) oldRootId_ = rootId_;
    rootId_ = next_root;
    // std::cout << "Setting new root proposal done " << std::endl;
  }

  void deleteOldRoot() {
    std::lock_guard<std::mutex> lock(rootMutex_);
    tree_.releaseSubTree(oldRootId_, rootId_);
    oldRootId_ = InvalidNodeId;
  }

  std::string printTree() const {
    // [TODO]: Only called when no search is performed!
    return tree_.printTree(0, tree_[rootId_]);
  }

 private:
  SearchTreeStorage tree_;

  mutable std::mutex rootMutex_;
  NodeId oldRootId_;
  NodeId rootId_;
};

} // namespace tree_search
} // namespace ai
} // namespace elf
