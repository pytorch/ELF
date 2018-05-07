# ELF Go C++ Interface

Go Game Rule part.

## Basic info
- in src_cpp/elfgames/go/base/board.h
- Define basic types and coordinates operation:
```cpp
 Coord: unsigned short;
 Stone: unsigned char;

 struct Info {color, id, next, last_placed};

 struct Group {color, ...};
 struct GroupId4 {};
 X(c); Y(c); getCoord(x, y);
```
- struct Board to represent board position:
```cpp
 struct Board {infos, groups, next player ply};
```
- board related functions for the Go-rule part: 
```cpp
FindAllValidMoves(board, ...);
showBoard(board, ...);
isGameEnd(board, ...); // two passes, or resign
isEye();
getLibertyMap(board, ...);

set_color(board, Coord c, Stone);
copyBoard(dst, src);
compareBoard(board1, board2);

StoneLibertyAnalysis();
isSuicideMove();
isSimpleKoViolation();
isSelfAtari();
checkLadderUseSearch();
checkLadder();
bool TryPlay(const board, x, y);
FindAllCandidateMoves();
float getTrompTaylorScore();
```

## Go State
- A class with all the above functions integrated;
- in src_cpp/elfgames/go/base/go_state.h
```cpp
class GoState {
 forward(c);
 checkMove();
 reset();
 board();
 getPly();
 terminated();
 nextPlayer();
 check_superko();
private:
 Board _board;
 deque<BoardHistory> _history;
};
```

## Go State Ext
- A class wrapped on GoState to support online behavior;
- in src_cpp/elfgames/go/go_state_ext.h
```cpp
class GoStateExt {
 public:
  dumpSgf();
  setRequest();
  setFinalValue();
  addMCTSPolicy();
  addPredictedValue();
  should Resign();
  forward(c);
 private:
  GoState _state;
  set<int> using_models_;
  float _last_value;
  GameOptions _options
  vector<CoordRecord> _mcts_policies;
  vector<float> _predicted_values;
};
```
- class GoStateExtOffline
```cpp
class GoStateExtOffline {
 fromRecord();
 switchRandomMove(rng);
 generateD4Code(rng);
 switchBeforeMove();
private:
 int _game_idx;
 GoState _state;
 BoardFeature _bf;
 GameOptions _options;
 vector<CoordRecord> _mcts_policies;
 vector<float> _predicted_values;
};
```

## 3. ``Board Feature``
- class BoardFeature
```cpp
class BoardFeature {
 enum Rot {};
 // apply random symmetry on the board in selfplay
 static BoardFeature RandomShuffle();
 state();
 setD4Group(), setD4Code(), getD4Code();

 Transform(); InvTransform();
 transform();
 // notice transform will be applied
 coord2Action();
 // notice inverse transform will be applied
 action2Coord();

 extract();
 extractAGZ();
};
 ```