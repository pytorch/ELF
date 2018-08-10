/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "elf/logging/IndexedLoggerFactory.h"
#include "elfgames/go/base/board.h"
#include "elfgames/go/base/common.h"

// Load the remaining part.
inline Coord str2coord(const std::string& s) {
  if (s.size() < 2)
    return M_PASS;
  size_t i = 0;
  while (i < s.size() && (s[i] == '\n' || s[i] == ' '))
    i++;
  if (i == s.size())
    return M_INVALID;
  int x = s[i] - 'a';

  i++;
  while (i < s.size() && (s[i] == '\n' || s[i] == ' '))
    i++;
  if (i == s.size())
    return M_INVALID;
  // if (x >= 9) x --;
  int y = s[i] - 'a';

  // tt
  if (!ON_BOARD(x, y))
    return M_INVALID;
  // if (y >= 9) y --;
  return OFFSETXY(x, y);
}

inline std::string coord2str(Coord c) {
  if (c == M_PASS)
    return "";
  int x = X(c);
  // if (x >= 8) x ++;
  int y = Y(c);
  // if (y >= 8) y ++;

  return std::string{static_cast<char>('a' + x), static_cast<char>('a' + y)};
}

inline std::string player2str(Stone player) {
  switch (player) {
    case S_WHITE:
      return "W";
    case S_BLACK:
      return "B";
    case S_UNKNOWN:
      return "U";
    case S_OFF_BOARD:
      return "O";
    default:
      return "-";
  }
}

inline std::string coord2str2(Coord c) {
  if (c == M_PASS)
    return "PASS";
  if (c == M_RESIGN)
    return "RESIGN";
  int x = X(c);
  if (x >= 8)
    x++;
  int y = Y(c);

  std::string s{static_cast<char>('A' + x)};
  return s + std::to_string(y + 1);
}

inline std::string coords2sgfstr(const std::vector<Coord>& moves) {
  std::string sgf = "(";
  for (size_t i = 0; i < moves.size(); i++) {
    std::string color = i % 2 == 0 ? "B" : "W";
    sgf += ";" + color + "[" + coord2str(moves[i]) + "]";
  }
  sgf += ")";
  return sgf;
}

inline std::vector<Coord> sgfstr2coords(const std::string& sgf) {
  std::vector<Coord> moves;
  if (sgf.empty() || sgf[0] != '(')
    return moves;

  size_t i = 1;
  while (true) {
    if (sgf[i] != ';')
      break;
    while (i < sgf.size() && sgf[i] != '[')
      i++;
    if (i == sgf.size())
      break;

    i++;
    size_t j = i;

    while (j < sgf.size() && sgf[j] != ']')
      j++;
    if (j == sgf.size())
      break;

    moves.push_back(str2coord(sgf.substr(i, j - i)));
    i = j + 1;
  }

  return moves;
}

struct SgfEntry {
  Coord move;
  Stone player;
  std::string comment;

  // All other (key, value) pairs.
  std::map<std::string, std::string> kv;

  // Tree structure.
  std::unique_ptr<SgfEntry> child;
  std::unique_ptr<SgfEntry> sibling;
};

struct SgfHeader {
  int rule;
  int size;
  float komi;
  int handi;
  std::string white_name, black_name;
  std::string white_rank, black_rank;
  std::string comment;

  Stone winner;
  float win_margin;
  std::string win_reason;

  void reset() {
    rule = 0;
    size = BOARD_SIZE;
    komi = 7.5;
    handi = 0;

    white_name = "";
    black_name = "";
    white_rank = "";
    black_rank = "";

    winner = S_OFF_BOARD;
    win_margin = 0.0;
  }
};

struct SgfMove {
  Stone player;
  Coord move;

  SgfMove() : player(S_OFF_BOARD), move(M_INVALID) {}

  SgfMove(Stone p, Coord m) : player(p), move(m) {}
};

// A library to load Sgf file for Go.
class Sgf {
 private:
  // sgf, a game tree.
  SgfHeader _header;
  std::unique_ptr<SgfEntry> _root;
  int _num_moves;
  std::shared_ptr<spdlog::logger> _logger;

  bool load_header(
      const char* s,
      const std::pair<int, int>& range,
      int* next_offset);

  static SgfEntry*
  load(const char* s, const std::pair<int, int>& range, int* next_offset);

 public:
  class iterator {
   public:
    iterator() : _curr(nullptr), _sgf(nullptr), _move_idx(-1) {}
    iterator(const Sgf& sgf)
        : _curr(sgf._root.get()), _sgf(&sgf), _move_idx(0) {}

    SgfMove getCurrMove() const {
      if (done())
        return SgfMove();
      else
        return SgfMove(_curr->player, _curr->move);
    }

    Coord getCoord() const {
      if (done())
        return M_INVALID;
      else
        return _curr->move;
    }

    std::string getCurrComment() const {
      return !done() ? _curr->comment : std::string("");
    }

    bool done() const {
      return _curr == nullptr;
    }

    iterator& operator++() {
      if (!done()) {
        _curr = _curr->sibling.get();
        _move_idx++;
      }
      return *this;
    }

    int getCurrIdx() const {
      return _move_idx;
    }

    int stepLeft() const {
      if (_sgf == nullptr)
        return 0;
      return _sgf->numMoves() - _move_idx - 1;
    }

    const Sgf& getSgf() const {
      return *_sgf;
    }

    std::vector<SgfMove> getForwardMoves(int k) const {
      auto iter = *this;
      std::vector<SgfMove> res;
      for (int i = 0; i < k; ++i) {
        res.push_back(iter.getCurrMove());
        ++iter;
      }
      return res;
    }

   private:
    const SgfEntry* _curr;
    const Sgf* _sgf;
    int _move_idx;
  };

  Sgf()
      : _num_moves(0),
        _logger(elf::logging::getLogger("elfgames::go::sgf::Sgf-", "")) {}
  bool load(const std::string& filename);
  bool load(const std::string& filename, const std::string& game);

  iterator begin() const {
    return iterator(*this);
  }

  Stone getWinner() const {
    return _header.winner;
  }
  int getHandicapStones() const {
    return _header.handi;
  }
  int getBoardSize() const {
    return _header.size;
  }
  int numMoves() const {
    return _num_moves;
  }

  const SgfHeader& getHeader() {
    return _header;
  }

  std::string printHeader() const;
  std::string printMainVariation();
};
