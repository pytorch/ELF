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
#include <set>

#include "elfgames/go/base/board.h"
#include "elfgames/go/base/board_feature.h"
#include "elfgames/go/base/test/test_utils.h"
#include "elfgames/go/sgf/sgf.h"

// test to load an empty board and check if it is empty
TEST(GoTest, testLoadBoard) {
  GoState b;
  std::string str = "";
  // str as an empty board
  for (int i = 0; i < 81; ++i)
    str += ".";
  loadBoard(b, str);

  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = toFlat(i, j);
      EXPECT_EQ(b.board()._infos[c].color, S_EMPTY);
    }
  }
}

// TODO: we don't have mini-Go's style koish check
TEST(GoTest, testKoish) {}

TEST(GoTest, testEyeish) {
  GoState b;

  std::string str(".XX...XXX");
  str += "X.X...X.X";
  str += "XX.....X.";
  str += "........X";
  str += "XXXX.....";
  str += "OOOX....O";
  str += "X.OXX.OO.";
  str += ".XO.X.O.O";
  str += "XXO.X.OO.";

  loadBoard(b, str);

  // test black eyes
  std::vector<std::string> B_eyes = {"aa", "bb", "ah", "hb", "ic"};
  for (auto str : B_eyes) {
    Coord c = str2coord(str);
    EXPECT_TRUE(isEye(&(b.board()), c, S_BLACK));
  }

  std::vector<std::string> W_eyes = {"ii", "hh", "ig"};
  for (auto str : W_eyes) {
    Coord c = str2coord(str);
    EXPECT_TRUE(isEye(&(b.board()), c, S_WHITE));
  }

  std::vector<std::string> not_eyes = {"bg", "ee"};
  for (auto str : not_eyes) {
    Coord c = str2coord(str);
    EXPECT_FALSE(isEye(&(b.board()), c, S_WHITE));
    EXPECT_FALSE(isEye(&(b.board()), c, S_BLACK));
  }
}

TEST(GoTest, testLibertyTrackerInit) {
  GoState b;
  Coord c;
  std::string str("X........");
  for (int i = 0; i < 8; ++i)
    str += ".........";

  loadBoard(b, str);

  // NOTICE: we have a +1 offset compared to mini-go
  EXPECT_EQ(b.board()._num_groups, 2);

  c = toFlat(0, 0);
  EXPECT_NE(b.board()._infos[c].id, 0);

  unsigned char id = b.board()._infos[c].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 2);
  EXPECT_EQ(b.board()._groups[id].color, S_BLACK);

  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0)};
  EXPECT_EQ(s1, s2);
}

TEST(GoTest, testPlaceStone) {
  GoState b;
  std::string str("X........");
  unsigned char id;
  for (int i = 0; i < 8; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(toFlat(1, 0));

  EXPECT_EQ(b.board()._num_groups, 2);
  EXPECT_NE(b.board()._infos[toFlat(0, 0)].id, 0);

  id = b.board()._infos[toFlat(0, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 3);
  id = b.board()._infos[toFlat(1, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 3);

  // check all stones in a group
  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0), toFlat(1, 0)};
  EXPECT_EQ(s1, s2);

  // TODO: check all liberties position
  EXPECT_EQ(b.board()._groups[id].color, S_BLACK);
}

TEST(GoTest, testPlaceStoneOppositeColor) {
  GoState b;
  unsigned char id;
  std::string str("X........");
  for (int i = 0; i < 8; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_WHITE);
  b.forward(toFlat(1, 0));

  // check number of groups
  EXPECT_EQ(b.board()._num_groups, 3);

  // check group id
  id = b.board()._infos[toFlat(0, 0)].id;
  EXPECT_NE(b.board()._groups[id].color, 0);
  id = b.board()._infos[toFlat(1, 0)].id;
  EXPECT_NE(b.board()._groups[id].color, 0);

  // check liberties
  id = b.board()._infos[toFlat(0, 0)].id;
  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0)};
  EXPECT_EQ(s1, s2);

  EXPECT_EQ(b.board()._groups[id].liberties, 1);
  id = b.board()._infos[toFlat(1, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 2);

  s1.clear();
  addSet(b, id, s1);
  s2.clear();
  s2.insert(toFlat(1, 0));
  EXPECT_EQ(s1, s2);

  // check group color
  id = b.board()._infos[toFlat(0, 0)].id;
  EXPECT_EQ(b.board()._groups[id].color, S_BLACK);
  id = b.board()._infos[toFlat(1, 0)].id;
  EXPECT_EQ(b.board()._groups[id].color, S_WHITE);
}

TEST(GoTest, testMergeMultipleGroups) {
  GoState b;
  unsigned char id;
  std::string str;
  str += ".X.......";
  str += "X.X......";
  str += ".X.......";
  for (int i = 0; i < 6; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(str2coord("bb"));

  // check group number
  assert(b.board()._num_groups == 2);

  // check group id
  id = b.board()._infos[toFlat(1, 1)].id;
  EXPECT_NE(id, 0);

  // check stones in this group
  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {
      toFlat(1, 0), toFlat(0, 1), toFlat(1, 1), toFlat(2, 1), toFlat(1, 2)};
  EXPECT_EQ(s1, s2);
  // check liberty in this group

  // check color
  id = b.board()._infos[toFlat(1, 1)].id;
  EXPECT_EQ(b.board()._groups[id].color, S_BLACK);

  // check liberty
  EXPECT_EQ(b.board()._groups[id].liberties, 6);
}

TEST(GoTest, testCaptureMultipleGroups) {
  GoState b;
  unsigned char id;

  std::string str;
  str += ".OX......";
  str += "OXX......";
  str += "XX.......";
  for (int i = 0; i < 6; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(toFlat(0, 0));

  // check group number
  EXPECT_EQ(b.board()._num_groups, 3);

  // check captured
  EXPECT_EQ(b.board()._b_cap, 2);

  // check corner stone
  id = b.board()._infos[toFlat(0, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 2);
  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0)};
  EXPECT_EQ(s1, s2);

  // check surrounding stones
  id = b.board()._infos[toFlat(2, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 7);

  s1.clear();
  s2.clear();
  addSet(b, id, s1);
  for (auto item :
       {toFlat(0, 2), toFlat(1, 1), toFlat(2, 1), toFlat(2, 0), toFlat(1, 2)})
    s2.insert(item);
  EXPECT_EQ(s1, s2);
}

TEST(GoTest, testCaptureStone) {
  GoState b;
  unsigned char id;
  std::string str;
  str += ".X.......";
  str += "XO.......";
  str += ".X.......";
  for (int i = 0; i < 6; ++i)
    str += ".........";

  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(toFlat(2, 1));

  // check group number
  EXPECT_EQ(b.board()._num_groups, 5);

  // check group id
  id = b.board()._infos[toFlat(1, 1)].id;
  EXPECT_EQ(id, 0);

  // check captured
  EXPECT_EQ(b.board()._b_cap, 1);
}

TEST(GoTest, testCaptureMany) {
  GoState b; // go board
  unsigned char id; // group id
  std::string str;
  str += ".XX......";
  str += "XOO......";
  str += ".XX......";
  for (int i = 0; i < 6; ++i)
    str += ".........";

  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(toFlat(3, 1));

  // check group number
  EXPECT_EQ(b.board()._num_groups, 5);

  // check group id
  id = b.board()._infos[toFlat(1, 1)].id;
  EXPECT_EQ(id, 0);

  // check captured
  EXPECT_EQ(b.board()._b_cap, 2);

  // check left group
  id = b.board()._infos[toFlat(0, 1)].id;
  assert(b.board()._groups[id].liberties == 3);
  std::set<Coord> s1;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 1)};
  EXPECT_EQ(s1, s2);

  // check right group
  id = b.board()._infos[toFlat(3, 1)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 4);
  s1.clear();
  s2.clear();
  addSet(b, id, s1);
  s2.insert(toFlat(3, 1));
  EXPECT_EQ(s1, s2);

  // check top group
  id = b.board()._infos[toFlat(1, 0)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 4);
  s1.clear();
  s2.clear();
  addSet(b, id, s1);
  for (auto item : {toFlat(1, 0), toFlat(2, 0)})
    s2.insert(item);
  EXPECT_EQ(s1, s2);

  // check bottom group
  id = b.board()._infos[toFlat(1, 2)].id;
  EXPECT_EQ(b.board()._groups[id].liberties, 6);
  s1.clear();
  s2.clear();
  addSet(b, id, s1);
  for (auto item : {toFlat(1, 2), toFlat(2, 2)})
    s2.insert(item);
  EXPECT_EQ(s1, s2);
}

TEST(GoTest, testSameFriendlyGroupNeighboringTwice) {
  GoState b;
  std::string str;
  std::set<Coord> s1;
  unsigned char id;
  str += "XX.......";
  str += "X........";
  for (int i = 0; i < 7; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_BLACK);
  b.forward(toFlat(1, 1));

  // check group number
  EXPECT_EQ(b.board()._num_groups, 2);

  // check group stones
  id = b.board()._infos[toFlat(0, 0)].id;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0), toFlat(0, 1), toFlat(1, 0), toFlat(1, 1)};
  EXPECT_EQ(s1, s2);

  // check liberty
  EXPECT_EQ(b.board()._groups[id].liberties, 4);
}

TEST(GoTest, testSameOpponentGroupNeighboringTwice) {
  GoState b;
  std::string str;
  std::set<Coord> s1;
  unsigned char id;
  str += "XX.......";
  str += "X........";
  for (int i = 0; i < 7; ++i)
    str += ".........";
  loadBoard(b, str);

  giveTurn(b, S_WHITE);
  b.forward(toFlat(1, 1));

  // check group number
  EXPECT_EQ(b.board()._num_groups, 3);

  // check black group
  id = b.board()._infos[toFlat(0, 0)].id;
  addSet(b, id, s1);
  std::set<Coord> s2 = {toFlat(0, 0), toFlat(0, 1), toFlat(1, 0)};
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(b.board()._groups[id].liberties, 2);

  // check white group
  id = b.board()._infos[toFlat(1, 1)].id;
  s1.clear();
  s2.clear();
  addSet(b, id, s1);
  for (auto item : {toFlat(1, 1)})
    s2.insert(item);
  EXPECT_EQ(s1, s2);
  EXPECT_EQ(b.board()._groups[id].liberties, 2);
}

TEST(GoTest, testPosition) {
  // test passing
  std::string str(".X.....OO");
  str += "X........";
  for (int i = 0; i < 7; ++i)
    str += ".........";
  GoState b1;
  loadBoard(b1, str);
  GoState b2(b1);
  b2.forward(0); // pass move
  EXPECT_TRUE(boardEqual(b1, b2));

  // TODO: test flip position
  // We do not need that, we can simply pass

  // Test normal moves
  giveTurn(b1, S_BLACK);
  Coord c;
  c = str2coord("ca");
  b1.forward(c);
  c = str2coord("ib");
  b1.forward(c);

  GoState b3;
  std::string str2(".XX....OO");
  str2 += "X.......O";
  for (int i = 0; i < 7; ++i)
    str2 += ".........";
  loadBoard(b3, str2);
  EXPECT_TRUE(boardEqual(b1, b3));
}

TEST(GoTest, testIsMoveSuicidal) {
  std::string str;
  str += "...O.O...";
  str += "....O....";
  str += "XO.....O.";
  str += "OXO...OXO";
  str += "O.XO.OX.O";
  str += "OXO...OOX";
  str += "XO.......";
  str += "......XXO";
  str += ".....XOO.";

  GoState b;
  Coord c;
  loadBoard(b, str);

  std::vector<std::string> suicidal_moves = {"ea", "he"};
  for (auto s : suicidal_moves) {
    giveTurn(b, S_BLACK);
    c = str2coord(s);
    EXPECT_FALSE(b.forward(c));
  }

  std::vector<std::string> nonsuicidal_moves = {"be", "ii", "aa"};
  for (auto s : nonsuicidal_moves) {
    giveTurn(b, S_BLACK);
    c = str2coord(s);
    EXPECT_TRUE(b.forward(c));
  }
}

TEST(GoTest, testLegalMoves) {
  std::string str;
  str += ".O.O.XOX.";
  str += "O..OOOOOX";
  str += "......O.O";
  str += "OO.....OX";
  str += "XO.....X.";
  str += ".O.......";
  str += "OX.....OO";
  str += "XX...OOOX";
  str += ".....O.X.";
  GoState b;
  loadBoard(b, str);
  Coord c;

  std::vector<std::string> illegalMoves = {"aa", "ea", "ia"};
  for (auto s : illegalMoves) {
    c = str2coord(s);
    giveTurn(b, S_BLACK);
    EXPECT_FALSE(b.forward(c));
  }

  std::vector<std::string> legalMoves = {"af", "gi", "ii", "hc"};
  for (auto s : legalMoves) {
    GoState b_(b);
    c = str2coord(s);
    giveTurn(b, S_BLACK);
    EXPECT_TRUE(b_.forward(c));
  }

  // Test all legal moves are legal
  AllMoves* allMoves = new AllMoves();
  FindAllValidMoves(&b.board(), S_BLACK, allMoves);
  for (int i = 0; i < allMoves->num_moves; ++i) {
    GoState b_(b);
    giveTurn(b, S_BLACK);
    EXPECT_TRUE(b_.forward(c));
  }

  // flip color
  flipColors(str);
  GoState b2;
  loadBoard(b2, str);
  for (auto s : illegalMoves) {
    c = str2coord(s);
    giveTurn(b, S_WHITE);
    EXPECT_FALSE(b.forward(c));
  }
  for (auto s : legalMoves) {
    GoState b_(b2);
    c = str2coord(s);
    giveTurn(b, S_WHITE);
    EXPECT_TRUE(b_.forward(c));
  }

  // test all white moves
  AllMoves* allMoves2 = new AllMoves();
  FindAllValidMoves(&b2.board(), S_WHITE, allMoves2);
  for (int i = 0; i < allMoves2->num_moves; ++i) {
    GoState b_(b2);
    giveTurn(b, S_WHITE);
    EXPECT_TRUE(b_.forward(c));
  }
}

TEST(GoTest, testMoveWithCaptures) {
  // Test move with captures
  std::string str;
  for (int i = 0; i < 5; ++i)
    str += ".........";
  str += "XXXX.....";
  str += "XOOX.....";
  str += "O.OX.....";
  str += "OOXX.....";
  GoState b;
  loadBoard(b, str);
  // Give the turn to black
  giveTurn(b, S_BLACK);
  // play the multi-capture move
  Coord c = str2coord("bh");
  b.forward(c);

  // actual board
  str = "";
  for (int i = 0; i < 5; ++i)
    str += ".........";
  str += "XXXX.....";
  str += "X..X.....";
  str += ".X.X.....";
  str += "..XX.....";
  GoState b2;
  loadBoard(b2, str);
  EXPECT_TRUE(boardEqual(b, b2));
}

TEST(GoTest, testKoMove) {
  GoState b;
  std::string str;
  Coord c;
  str += ".OX......";
  str += "OX.......";
  for (int i = 0; i < 7; ++i)
    str += ".........";
  loadBoard(b, str);

  // give the turn to BLACK
  giveTurn(b, S_BLACK);
  c = str2coord("aa");
  b.forward(c);

  str = "";
  str += "X.X......";
  str += "OX.......";
  for (int i = 0; i < 7; ++i)
    str += ".........";
  GoState b2;
  loadBoard(b2, str);
  EXPECT_TRUE(boardEqual(b, b2));

  // test ko-move conflict
  c = str2coord("ba");
  EXPECT_FALSE(b.forward(c));

  // retake ok, after two other moves
  b.forward(str2coord("ii"));
  b.forward(str2coord("ih"));
  EXPECT_TRUE(b.forward(str2coord("ba")));
}

TEST(GoTest, testIsGameOver) {
  GoState b;
  EXPECT_FALSE(isGameEnd(&b.board()));

  b.forward(0);
  b.forward(0);

  EXPECT_TRUE(isGameEnd(&b.board()));
}

TEST(GoTest, testScoring) {
  std::string str("");
  str += ".XX......";
  str += "OOXX.....";
  str += "OOOX...X.";
  str += "OXX......";
  str += "OOXXXXXX.";
  str += "OOOXOXOXX";
  str += ".O.OOXOOX";
  str += ".O.O.OOXX";
  str += "......OOO";
  GoState b;
  loadBoard(b, str);

  float score = b.evaluate(6.5);
  EXPECT_EQ(score, 1.5);

  str[0] = 'X';
  GoState b2;
  loadBoard(b2, str);
  score = b2.evaluate(6.5);
  EXPECT_EQ(score, 2.5);
}

TEST(GoTest, testReplayPosition) {
  GoState b;
  std::string s;
  s += "B[fd];W[cf];B[eg];W[dd];B[dc];W[cc];B[de];W[cd];";
  s += "B[ed];W[he];B[ce];W[be];B[df];W[bf];B[hd];W[ge];";
  s += "B[gd];W[gg];B[db];W[cb];B[cg];W[bg];B[gh];W[fh];";
  s += "B[hh];W[fg];B[eh];W[ei];B[di];W[fi];B[hg];W[dh];";
  s += "B[ch];W[ci];B[bh];W[ff];B[fe];W[hf];B[id];W[bi];";
  s += "B[ah];W[ef];B[dg];W[ee];B[di];W[ig];B[ai];W[ih];";
  s += "B[fb];W[hi];B[ag];W[ab];B[bd];W[bc];B[ae];W[ad];";
  s += "B[af];W[bd];B[ca];W[ba];B[da];W[ie]";

  size_t i = 0;
  while (i <= s.size() / 6) {
    std::string subs = s.substr(i * 6 + 2, 2);
    b.forward(str2coord(subs));
    i += 1;
  }

  std::string str2;
  str2 += ".OXX.....";
  str2 += "O.OX.X...";
  str2 += ".OOX.....";
  str2 += "OOOOXXXXX";
  str2 += "XOXXOXOOO";
  str2 += "XOOXOO.O.";
  str2 += "XOXXXOOXO";
  str2 += "XXX.XOXXO";
  str2 += "X..XOO.O.";
  GoState b2;
  loadBoard(b2, str2);
  EXPECT_TRUE(boardEqual(b, b2));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
