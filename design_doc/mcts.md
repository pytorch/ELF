# MCTS in ELF OpenGo

- MCTS is generally implemented in C++ with templates.
- Three important concepts: **Actor**, **Action**, **State**.
- General implementations available in the directory src_cpp/elf/ai/tree_search;
- Go-specific available in src_cpp/elfgames/go/mcts/
- Two modes are supported: a multi-threading mode for training (used in selfplay games), and a pseudo-multi-threading batch mode for online games (e.g., gtp).

## Actor
- class MCTSActor, a go specific implementation in src_cpp/elfgames/go/mcts/mcts.h
- Important member function **evaluate** pre-evaluate, get feature and call neural network if necessary, post-process and save in resp;
```cpp
// evaluate a single state
void evaluate(const GoState& s, NodeResponse* resp);

// batch-mode
void evaluate(const vector<const GoState*>& states,
      vector<NodeResponse>* p_resps);
```
- Important member  **pi2response()** will call action2Coord() with inv-transform considered, remove invalid moves, normalize;
- Other members:
```cpp
public:
 forward(s, a);
 reward(s, value);
private:
 unique_ptr<AI> ai_; // client to run neural network
 get_extractor()
 pre_evaluate(); // check terminated, evaluate with komi;
 ai_->act(bf, &reply); // will call neural net
 post_nn(); // pi2response
```

## MCTS Go AI
- class **MCTSGoAI**, derived from class **MCTSAI_T<MCTSActor>**. In the class we add following members:
```cpp
class MCTSGoAI : public MCTSAI_T<MCTSActor> {
public:
  float getValue();
  MCTSPolicy<Coord> getMCTSPolicy();
};
```
- class **MCTSAI** can be found in the folder src_cpp/elf/ai/tree_search/mcts.h. It is derived from class AI_T<State, Action>
```cpp
template <typename Actor>
class MCTSAI_T : public AI_T<typename Actor::State, typename Actor::Action> {
public:
 TreeSearch* getEngine();
 // act() call ts_->run(), which return best action
 bool act(); 
 bool actPolicyOnly();
 // reset Tree;
 bool endGame();
 const MCTSResult& getLastResult() const;
 string getCurrentTree():
private:
 TSOptions options_;
 unique_ptr<TreeSearch> ts_;
 MCTSResult lastResult_;
 resetTree();
 
 advanceMove();
};
```
- Important function **advanceMove()** if move is valid, ts_ will call treeAdvance(), which will recursively remove not selected nodes; otherwise reset tree;
- The very basic class AI\_T
```cpp
<S, A>
class AI_T {
public:
 setID();
 getID();
 // multi-threading mode
 virtual bool act(S&, A*);
 // batch mode
 virtual bool act_batch(
      const vector<const S*>& /*batch_s*/,
      const vector<A*>& /*batch_a*/)
 endGame(S&);
private:
 int id_;
};
```

## Tree Search Details
- class **TreeSearchSingleThread** in folder src_cpp/elf/ai/tree_search/tree_search.h is for training multi-threading execution.
```cpp
template <typename State, typename Action>
class TreeSearchSingleThreadT {
 // constructor...

 notifyReady();

 visit(); // actor evaluate a new node, expand if necessary
 
 /* run() will iterates n_rollout times: 
    while visit(node)
      find move
      add virtual loss
      followEdge
      allocate state
    get reward
   update edge states
 */
 // run() will call batch_rollouts()
 bool run(id, bool, actor, n_rollout); get root node, 
 
 int threadId_;
 float get_reward();
 // actor forward and create a new state
 bool allocate_state();

 /* run batch_size times single_rollout() to get 
    trajectories
    send the batch of nodes to evaluate with CNN
    get reward
    update edge states
  */
 template <typename Actor>
  size_t batch_rollouts(
      const RunContext& ctx,
      Node* root,
      Actor& actor,
      SearchTree& search_tree);

 /* single_rollout() will iterates 1) to 4) until expanding an unvisited node visit(node):
    1) find move; 2) add virtual loss; 3) followEdge; 4) allocate state;
  it returns the trajectory;
    
 */
 Traj single_rollout(
      RunContext ctx,
      Node* root,
      Actor& actor,
      SearchTree& search_tree);
```
- class **TreeSearch** is based on class **TreeSearchSingleThread**
```cpp
template <typename State, typename Action, typename Actor>
class TreeSearchT {
public:
 // emplace back all Singe Thread tree-search in the threadPool;
 getActor(int i);
 getNumActors();
 runPolicyOnly();
 treeAdvance();
 clear();
 stop();
 // get root -> notify search -> wait until count -> reset -> chooseAction;
 run(root_state);
private:
 vector<thread> threadPool_
 vector<TreeSearchSingleThread*> treeSearches_;
 vector<Actor*> actors_;
 SearchTree searchTree_;
 TSOptions options_;
 atomic<bool> stopSearch_;
 concurrency::Counter<size_t> treeReady_;
 concurrency::Counter<size_t> countStoppedThread;
 notifySearches();
 setRootState();
 // get results from the hash table SA
 MCTSResult chooseAction();
};
```

## Tree and Nodes
- Base class **NodeBase**
```cpp
template<State>
class NodeBase {
public:
 getStatePtr()
 setStateIfUnset()
private:
 mutex lockState_
 State* state_
 StateType stateType_
};
```
- Class **Node**
```cpp
template<State, Action>
class Node : NodeBase<State>
 getStateActions();
 getNumVisits(), getValue(), getMeanUnsignedQ(), isVisited()
 enhanceExploration();
 expandIfNecessary(func); // update state action tables, value, visited and flipped Q sign
 findMove(alg, depth, action); // act with argmax UCT
 addVirtualLoss(action, virtual loss);
 updateEdgeStats(); // backup value
 followEdge(action, tree); // tree adds a new node
private:
 mutex lockNode
 atomic<bool> visited_
 unordered_map<Action, EdgeInfo> stateActions
 unordered_map<Action, mutex*> lockStateActions
 atomic<int> numVisits
 float V_, unsignedMeanQ_, unsignedParentQ
 bool flipQSign_ = false
 struct BestAction {Action action, float max_score, float unsigned_q, int total_visits}
 
 // use UCT to select best action
 BestAction UCT(alg);
```

- class **SearchTree** built on class **Node**
```cpp
template<State, Action>
class SearchTree {
public:
 clear()
 treeAdvance() // will free all unused nodes recursively before moving the next root;
 Node* getRootNode()
 NodeId addNode(float); // add a new node with parent Q?
 freeNode(NodeId);
 operator[](NodeId i); // get the node by key
 printTree();
private:
 unordered_map<NodeId, unique_ptr<Node>> allocatedNodes;
 getNode(NodeId)
 allocateRoot(): addNode(0.0) as root
};
```