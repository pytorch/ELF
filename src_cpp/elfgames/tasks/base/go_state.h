/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <deque>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <vector>

//#include "elfgames/go/sgf/sgf.h"

#include "board.h"
#include "board_feature.h"

//class HandicapTable {
// private:
//  // handicap table.
//  std::unordered_map<int, std::vector<Coord>> _handicaps;
//
// public:
//  HandicapTable();
//  void apply(int handi, Board* board) const;
//};

/*
inline std::vector<bool>
simple_flood_fill(const Board& b, Stone player, std::ostream* oo = nullptr) {
  std::queue<Coord> q;
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (b._infos[c].color == player) {
        q.push(c);
      }
    }
  }

  if (oo != nullptr)
    *oo << "For player " << player2str(player) << ": #stone: " << q.size()
        << std::endl;

  std::vector<bool> open(BOARD_SIZE * BOARD_SIZE, false);
  std::vector<bool> f(BOARD_SIZE * BOARD_SIZE, false);

  int counter = 0;
  while (!q.empty()) {
    Coord c = q.front();
    // if (oo != nullptr) *oo << coord2str2(c) << " ";
    q.pop();
    f[EXPORT_OFFSET(c)] = true;
    counter++;

    FOR4(c, _, cc)
    int offset = EXPORT_OFFSET(cc);
    if (b._infos[cc].color == S_EMPTY && !open[offset]) {
      open[offset] = true;
      q.push(cc);
    }
    ENDFOR4
  }

  if (oo != nullptr)
    *oo << std::endl
        << "For player " << player2str(player) << ": territory = " << counter
        << std::endl;
  return f;
}

inline int simple_tt_scoring(const Board& b, std::ostream* oo = nullptr) {
  // No dead stone considered.
  // [TODO] Can be more efficient with bitset.
  // if (oo != nullptr) *oo << "In tt scoring" << endl;
  std::vector<bool> black = simple_flood_fill(b, S_BLACK, oo);
  std::vector<bool> white = simple_flood_fill(b, S_WHITE, oo);

  int black_v = 0, white_v = 0;
  for (size_t i = 0; i < black.size(); ++i) {
    if (black[i] && !white[i])
      black_v++;
    else if (white[i] && !black[i])
      white_v++;
  }

  if (oo != nullptr)
    *oo << "black_v: " << black_v << ", white: " << white_v << std::endl;
  return black_v - white_v;
}
*/
/*class ChouFleurState {
 public:
  ChouFleurState() {
    reset();
  }
  bool forward(const Coord& c);
  bool checkMove(const Coord& c) const;

  void setFinalValue(float final_value) {
    _final_value = final_value;
    _has_final_value = true;
  }
  float getFinalValue() const {
    return _final_value;
  }
  bool HasFinalValue() const {
    return _has_final_value;
  }

  void reset();
  void applyHandicap(int handi);

  ChouFleurState(const ChouFleurState& s)
      : _history(s._history),
        _board_hash(s._board_hash),
        _moves(s._moves),
        _final_value(s._final_value),
        _has_final_value(s._has_final_value) {
    copyBoard(&_board, &s._board);
  }

  static HandicapTable& handi_table() {
    return _handi_table;
  }

  const Board& board() const {
    return _board;
  }

  // Note that ply started from 1.
  bool justStarted() const {
    return _board._ply == 1;
  }

  int getPly() const {
    return _board._ply;
  }
  bool isTwoPass() const {
    return _board._last_move == M_PASS && _board._last_move2 == M_PASS;
  }

  bool terminated() const {
    return isTwoPass() || getPly() >= BOARD_MAX_MOVE || _check_superko();
  }

  Coord lastMove() const {
    return _board._last_move;
  }
  Coord lastMove2() const {
    return _board._last_move2;
  }
  Stone nextPlayer() const {
    return _board._next_player;
  }

  bool moves_since(size_t* next_move_number, std::vector<Coord>* moves) const {
    if (*next_move_number > _moves.size()) {
      // The move number is not right.
      return false;
    }
    moves->clear();
    for (size_t i = *next_move_number; i < _moves.size(); ++i) {
      moves->push_back(_moves[i]);
    }
    *next_move_number = _moves.size();
    return true;
  }

  uint64_t getHashCode() const {
    return _board._hash;
  }

  const std::vector<Coord>& getAllMoves() const {
    return _moves;
  }
  std::string getAllMovesString() const {
    std::stringstream ss;
    for (const Coord& c : _moves) {
      ss << "[" << coord2str2(c) << "] ";
    }
    return ss.str();
  }

  std::string showBoard() const {
 //   char buf[2000]; showBoard2Buf(&_board, SHOW_LAST_MOVE, buf); return std::string(buf) + "\n" + "Last move: " + coord2str2(lastMove()) + ", nextPlayer: " + (nextPlayer() == S_BLACK ? "Black" : "White") + "\n";
  }

  float evaluate(float komi, std::ostream* oo = nullptr) const {
    exit(-1);
    float final_score = 0.0;
    if (_check_superko()) {
      final_score = nextPlayer() == S_BLACK ? 1.0 : -1.0;
    } else {
      final_score = (float)simple_tt_scoring(_board, oo) - komi;
    }

    // cout << "Calling evaluate on the current situation, final score: " <<
    // final_score << endl;
    // cout << showBoard();
    return final_score;
  }

  // TODO: not a good design..
  const std::deque<BoardHistory>& getHistory() const {
    return _history;
  }

 protected:
  Board _board;
  std::deque<BoardHistory> _history;

  struct _BoardRecord {
    Board::Bits bits;
  };

  std::unordered_map<uint64_t, std::vector<_BoardRecord>> _board_hash;

  std::vector<Coord> _moves;
  float _final_value = 0.0;
  bool _has_final_value = false;

//  static HandicapTable _handi_table;

  bool _check_superko() const;
  void _add_board_hash(const Coord& c);
};*/

struct ChouFleurReply {
  const BoardFeature& bf;
  Coord c;
  std::vector<float> pi;
  float value = 0;
  // Model version.
  int64_t version = -1;

  ChouFleurReply(const BoardFeature& bf) : bf(bf), pi(StateForChouFleurNumActions, 0.0) {}
};
