/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "elfgames/go/base/board.h"
#include "elfgames/go/base/common.h"
#include "elfgames/go/base/go_state.h"

Coord toFlat(const int x, const int y) {
  Coord c;
  c = (y + 1) * BOARD_EXPAND_SIZE + x + 1;
  return c;
}

// revert colors in a string representing a board
void flipColors(std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == 'X')
      s[i] = 'O';
    else if (s[i] == 'O')
      s[i] = 'X';
  }
}

Stone getTurn(const GoState& b) {
  if (b.getPly() % 2 == 0)
    return S_WHITE;
  else if (b.getPly() % 2 == 1)
    return S_BLACK;
  else
    throw "Unknown player";
}

void giveTurn(GoState& b, const Stone s) {
  if (s == S_BLACK) {
    if (getTurn(b) == S_WHITE)
      b.forward(0);
  } else if (s == S_WHITE) {
    if (getTurn(b) == S_BLACK)
      b.forward(0);
  } else
    throw "Unknown player";
}

void loadBoard(GoState& b, const std::string& str) {
  assert(str.size() == BOARD_SIZE * BOARD_SIZE);
  int x, y;
  Coord c;
  for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
    assert(str[i] == '.' || str[i] == 'X' || str[i] == 'O');

    x = i % BOARD_SIZE;
    y = i / BOARD_SIZE;
    c = toFlat(x, y);
    if (str[i] == '.')
      continue;
    else {
      // make one pass if not the player to play
      if ((str[i] == 'X' && getTurn(b) == S_WHITE) ||
          (str[i] == 'O' && getTurn(b) == S_BLACK))
        b.forward(0);
      b.forward(c);
    }
  }
}

bool boardEqual(const GoState& b1, const GoState& b2) {
  for (int j = 0; j < BOARD_SIZE; ++j) { // y
    for (int i = 0; i < BOARD_SIZE; ++i) { // x
      Coord c = toFlat(i, j);
      if (b1.board()._infos[c].color != b2.board()._infos[c].color) {
        return false;
      }
    }
  }
  return true;
}

// add all the coords with the specified group id
// to set s
void addSet(const GoState& b, const unsigned char id, std::set<Coord>& s) {
  Coord c = b.board()._groups[id].start;

  while (s.find(c) == s.end()) {
    s.insert(c);
    // c move to next
    c = b.board()._infos[c].next;
    if (c == 0)
      break;
  }
}
