# Game Base Interface

## Go Game Base:
- class GoGameBase is the base class, from which selfplay and training mode classes are derived
- public members:
```cpp
  mainLoop() // while client doesn't stop game, act();
  virtual act()=0;
```
- private members:
```cpp
  GameClient* client_;
  rng;
  _options;
  _context_options;
```

## Go Game Self Play
- class ``GoGameSelfPlay`` derived from GoGameBase
- Public Members:
```cpp
  // get ai, board feature, ai.act(), Bind State to Functions, send and wait; make diverse move; mcts update info; forward(c); terminate and finish game if needed;
  act(); 
  
  showBoard()
  GoStateExt _state_ext
  EvalCtrl* eval_ctrl_
  unique_ptr<MCTSGoAI> _ai, _ai2;
  unique_ptr<AI> _human_player;
```
- private members:
```cpp
  init_ai()
  mcts_make_diverse_move()
  mcts_update_info()

  restart()
```
- provided Python interface:
```python
class GoGameSelfplay()
  def showBoard()
  def getNextPlayer()
  def getLastMove()
  def getScore()
  def getLastScore()
```

## Go Game Train
- class ``GoGameSelfPlay`` derived from GoGameBase
- Public members:
```cpp
 // get sampler, sample, randomly pick move from record, setD4Code(), BindStateToFunctions("train")
 act();
```