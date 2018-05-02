/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <chrono>
#include <fstream>

#include "board_feature.h"
#include "go_state.h"

static std::vector<std::string> split(const std::string& s, char delim) {
  std::stringstream ss(s);
  std::string item;
  std::vector<std::string> elems;
  while (getline(ss, item, delim)) {
    elems.push_back(std::move(item));
  }
  return elems;
}

static Coord s2c(const std::string& s) {
  int row = s[0] - 'A';
  if (row >= 9)
    row--;
  int col = stoi(s.substr(1)) - 1;
  return getCoord(row, col);
}

HandicapTable::HandicapTable() {
  // darkforestGo/cnnPlayerV2/cnnPlayerV2Framework.lua
  // Handicap according to the number of stones.
  const std::map<int, std::string> handicap_table = {
      {2, "D4 Q16"},
      {3, "D4 Q16 Q4"},
      {4, "D4 Q16 D16 Q4"},
      {5, "*4 K10"},
      {6, "*4 D10 Q10"},
      {7, "*4 D10 Q10 K10"},
      {8, "*4 D10 Q10 K16 K4"},
      {9, "*8 K10"},
      // {13, "*9 G13 O13 G7 O7", "*9 C3 R3 C17 R17" },
  };
  for (const auto& pair : handicap_table) {
    _handicaps.insert(std::make_pair(pair.first, std::vector<Coord>()));
    for (const auto& s : split(pair.second, ' ')) {
      if (s[0] == '*') {
        const int prev_handi = stoi(s.substr(1));
        auto it = _handicaps.find(prev_handi);
        if (it != _handicaps.end()) {
          _handicaps[pair.first] = it->second;
        }
      }
      _handicaps[pair.first].push_back(s2c(s));
    }
  }
}

void HandicapTable::apply(int handi, Board* board) const {
  if (handi > 0) {
    auto it = _handicaps.find(handi);
    if (it != _handicaps.end()) {
      for (const auto& ha : it->second) {
        PlaceHandicap(board, X(ha), Y(ha), S_BLACK);
      }
    }
  }
}

///////////// GoState ////////////////////
bool GoState::forward(const Coord& c) {
  if (c == M_INVALID) {
    throw std::range_error("GoState::forward(): move is M_INVALID");
  }
  if (terminated())
    return false;

  GroupId4 ids;
  if (!TryPlay2(&_board, c, &ids))
    return false;

  _add_board_hash(c);

  Play(&_board, &ids);

  _moves.push_back(c);
  _history.emplace_back(_board);
  if (_history.size() > MAX_NUM_AGZ_HISTORY)
    _history.pop_front();
  return true;
}

bool GoState::_check_superko() const {
  // Check superko rule.
  // need to check whether last move is pass or not.
  if (lastMove() == M_PASS)
    return false;

  uint64_t key = _board._hash;
  auto it = _board_hash.find(key);
  if (it != _board_hash.end()) {
    for (const auto& r : it->second) {
      if (isBitsEqual(_board._bits, r.bits))
        return true;
    }
  }
  return false;
}

void GoState::_add_board_hash(const Coord& c) {
  if (c == M_PASS)
    return;

  uint64_t key = _board._hash;
  auto& r = _board_hash[key];
  r.emplace_back();
  copyBits(r.back().bits, _board._bits);
}

bool GoState::checkMove(const Coord& c) const {
  GroupId4 ids;
  if (c == M_INVALID)
    return false;
  return TryPlay2(&_board, c, &ids);
}

void GoState::applyHandicap(int handi) {
  _handi_table.apply(handi, &_board);
}

void GoState::reset() {
  clearBoard(&_board);
  _moves.clear();
  _board_hash.clear();
  _history.clear();
  _final_value = 0.0;
  _has_final_value = false;
}

HandicapTable GoState::_handi_table;
