/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * Acknowledgement:
 * These tests are loosely ported from the MiniGo project's excellent unit
 * tests. https://github.com/tensorflow/minigo
 */

#include <gtest/gtest.h>

#include "elf/ai/tree_search/tree_search_base.h"
#include "elf/ai/tree_search/tree_search_node.h"
#include "elf/ai/tree_search/tree_search_options.h"
#include "elfgames/go/base/board.h"
#include "elfgames/go/base/go_state.h"
#include "elfgames/go/base/test/test_utils.h"
#include "elfgames/go/sgf/sgf.h"

using State = GoState;
using Action = Coord;
using Node = elf::ai::tree_search::NodeT<State, Action>;
using NodeId = elf::ai::tree_search::NodeId;
using SearchTree = elf::ai::tree_search::SearchTreeT<State, Action>;
using SearchAlgoOptions = elf::ai::tree_search::SearchAlgoOptions;
using EdgeInfo = elf::ai::tree_search::EdgeInfo;
using NodeResponse = elf::ai::tree_search::NodeResponseT<Coord>;

namespace elf {
namespace ai {
namespace tree_search {
class NodeTest : public Node {
 public:
  NodeTest(float unsigned_parent_q, EdgeInfo* parEdge = nullptr)
      : Node(unsigned_parent_q), edgeInfo(parEdge) {}

  // enable modifying the StateAction pairs for unit-testing only
  std::unordered_map<Action, EdgeInfo>& getPublicStateActions() {
    return stateActions_;
  }

  std::unordered_map<Action, std::unique_ptr<std::mutex>>&
  getPublicLockStateActions() {
    return lockStateActions_;
  }

  void visit(float v) {
    V_ = v;
    visited_ = true;
    numVisits_++;
    if (edgeInfo != nullptr)
      edgeInfo->num_visits++;
  }

  // parent edge info
  EdgeInfo* edgeInfo;

  EdgeInfo& getParEdge() {
    return *edgeInfo;
  }

  EdgeInfo& getEdge(Action a) {
    return stateActions_.at(a);
  }

  // insert action
  void insertAction(Action a, float p) {
    EdgeInfo* tmp_info = new EdgeInfo(p);
    stateActions_.insert(std::make_pair(a, *tmp_info));
    lockStateActions_[a].reset(new std::mutex());
  }

  // get Q value of self by saving parent
  float Q() {
    assert(edgeInfo != nullptr);
    float reward = edgeInfo->reward;
    int num_visits = edgeInfo->num_visits;
    return reward / num_visits;
  }

  // set flip
  void set_flip(bool flip) {
    flipQSign_ = flip;
  }
};

class TestActor {
 public:
  void evaluate(const State& s, NodeResponse* resp) {
    // to avoid unused parameter warning
    EXPECT_FALSE(s.terminated());
    // resp.pi: vector of <Action, prob> pairs
    // resp.value: value of the state
    resp->pi.push_back(std::make_pair(17, .1));
    resp->value = 0.;
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf

using NodeTest = elf::ai::tree_search::NodeTest;
using TestActor = elf::ai::tree_search::TestActor;

namespace {

static constexpr size_t kBoardRegion = BOARD_SIZE * BOARD_SIZE;

// Automatically satisfied
// skip this unit-test
TEST(MctsTest, testActionFlip) {}

TEST(MctsTest, testSelectLeaf) {
  // initialze root
  NodeTest* root = new NodeTest(0.);

  // uniform initialization
  auto& saPairs = root->getPublicStateActions();
  for (int i = 0; i < 20; ++i) {
    root->insertAction(i + 1, .02);
  }

  // modify 1 node
  saPairs.at(3).prior_probability = 0.4;

  SearchAlgoOptions algOpt;
  Action action;
  root->findMove(algOpt, 1, &action);
  EXPECT_EQ(action, 3);
}

TEST(MctsTest, testBackupIncorporateResults) {
  State b;
  std::string str;
  float reward;
  str += ".XO.XO.OO";
  str += "X.XXOOOO.";
  str += "XXXXXOOOO";
  str += "XXXXXOOOO";
  str += ".XXXXOOO.";
  str += "XXXXXOOOO";
  str += ".XXXXOOO.";
  str += "XXXXXOOOO";
  str += "XXXXOOOOO";
  loadBoard(b, str);

  // dummy node, pointing to root
  NodeTest dummy(0.);
  dummy.insertAction(0, 0.);

  NodeTest root(0., &dummy.getEdge(0));
  EXPECT_EQ(root.getNumVisits(), 0);

  // EdgeInfo to udpate
  for (Action i = 0; i < ::kBoardRegion + 1; ++i)
    root.insertAction(i, .02);
  root.visit(0.);

  // white win!
  reward = -1.;
  Action action;
  SearchAlgoOptions algOpt;
  algOpt.use_prior = true;
  EXPECT_TRUE(root.findMove(algOpt, 1, &action));
  EXPECT_TRUE(root.updateEdgeStats(action, reward, 0.));
  EXPECT_TRUE(dummy.updateEdgeStats(0, reward, 0.));
  EXPECT_EQ(root.getNumVisits(), 2);
  // test root q-value
  // our implementation does not have q returned:
  // copied from UCT() from tree_search_node.h
  EXPECT_FLOAT_EQ(root.Q(), -.5);
  // Note: in mini-Go, there is a +1, so q = -1/3
  // our implementation: q = -1/2

  /* We're assuming that select_leaf() returns a leaf like:
     root
       \
       leaf
         \
         leaf2
     which happens in this test because root is W to play and leaf was a W win.
  */
  NodeTest leaf(0., &root.getEdge(action));
  Action action2 = 5;
  leaf.insertAction(action2, 1.);
  EXPECT_TRUE(leaf.updateEdgeStats(action2, -.2, 0.));
  EXPECT_TRUE(root.updateEdgeStats(action, -.2, 0.));
  EXPECT_TRUE(dummy.updateEdgeStats(0, -.2, 0.));
  // suppose white's turn
  root.set_flip(true);

  Action action3, action4;
  root.findMove(algOpt, 1, &action3);
  leaf.findMove(algOpt, 2, &action4);
  EXPECT_EQ(action, action3);
  EXPECT_EQ(action2, action4);
  EXPECT_EQ(root.getParEdge().num_visits, 3);
  EXPECT_FLOAT_EQ(root.Q(), -0.4);

  EXPECT_EQ(leaf.getParEdge().num_visits, 2);
  EXPECT_FLOAT_EQ(leaf.Q(), -0.6);
}

// check root add child twice,
// and assert the same child inserted
TEST(MctsTest, testAddChild) {
  SearchTree tree;
  TestActor actor;
  // will call tree.clear() in constructor
  // which assigns a root node

  // Notice: in mini-Go
  // addnode is called by a node
  // In ELF
  // We first call expandIfNecessary() to insert SA()
  // Then followEdge is called to create the node
  Node* root = tree.getRootNode();
  State s;
  // add to root Q(s, a) hash-table
  auto func = [&](const Node* n, NodeResponse* resp) {
    // to avoid unused parameter warning
    assert(n != nullptr);
    actor.evaluate(s, resp);
  };
  root->expandIfNecessary(func);
  auto& stateActions = root->getStateActions();
  auto it = stateActions.find(17);
  EXPECT_NE(it, stateActions.end());
  NodeId id = root->followEdge(17, tree);
  EXPECT_EQ(id, 1);
}

// same as test_add_child()
// test in case of deep copy
TEST(MctsTest, testAddChildIdempotency) {
  SearchTree tree;
  TestActor actor;
  // will call tree.clear() in constructor
  // which assigns a root node

  // Notice: in mini-Go
  // addnode is called by a node
  // In ELF
  // We first call expandIfNecessary() to insert SA()
  // Then followEdge is called to create the node
  Node* root = tree.getRootNode();
  State s;
  // add to root Q(s, a) hash-table
  auto func = [&](const Node* n, NodeResponse* resp) {
    assert(n != nullptr);
    actor.evaluate(s, resp);
  };
  root->expandIfNecessary(func);
  NodeId id = root->followEdge(17, tree);

  // re-add the node to root
  root->expandIfNecessary(func);
  id = root->followEdge(17, tree);

  // The same node (will not create twice)
  EXPECT_EQ(id, 1);
}

TEST(MctsTest, testDontPickUnexpandedChild) {
  // SearchTree tree;
  NodeTest root(0.);

  // initialize root.stateActions_
  for (int i = 0; i < 20; ++i) {
    root.insertAction(i, .001);
  }

  // modify 1 node
  auto& saPairs = root.getPublicStateActions();
  saPairs.at(17).prior_probability = 0.999;

  SearchAlgoOptions algOpt;
  Action action;
  EXPECT_TRUE(root.findMove(algOpt, 1, &action));
  EXPECT_EQ(action, 17);

  // add virtual loss -0.5
  // We should add virtual loss = -1.
  // However, the N + 1 will influence result
  // MiniGo virtual loss will reduce to 0.5
  EXPECT_TRUE(root.addVirtualLoss(17, -.5));
  Action action2;
  root.findMove(algOpt, 1, &action2);
  EXPECT_EQ(action2, 17);
}

// not test-friendly in ELF
TEST(MctsTest, testNeverSelectIllegalMoves) {
  // in elfgames/go/mcts.h
  // class MCTSActor
  // has member function bool pi2response()
  // bool valid = s.checkMove(v.first))
  // will not be added in NodeResponse
}

TEST(MctsTest, testDoNotExplorePastFinish) {
  // test if two passes are played
  // game over
  // shouldn't explore any more
  // implemented in go/mcts/mcts.h
  std::string str;
  for (int i = 0; i < 9; ++i)
    str += ".........";
  State s;
  loadBoard(s, str);
  s.forward(0);
  s.forward(0);
  EXPECT_TRUE(s.terminated());

  // MCTSActor actor;
  // has a member-function as pre_evaluate()
  // will return EVAL_DONE
  // Then actor.evaluate() will not call NN
  // especially, in NodeResponse resp
  // resp.pi will be empty
  // so findmove() has no more childen to expand
}
} // anonymous namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
