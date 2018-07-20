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

#include "elfgames/go/base/go_state.h"
#include "elfgames/go/base/test/test_utils.h"
#include "elfgames/go/sgf/sgf.h"

// notice we have different translate system
// we do not have a separate function for this
// and a (x, y) -> (y, x) flip difference from
TEST(SgfTest, testTranslateSgfMove) {
  EXPECT_EQ(str2coord("db"), toFlat(3, 1));
  EXPECT_EQ(str2coord("aa"), toFlat(0, 0));
  EXPECT_EQ(str2coord(""), 0);
}

// in Mini-Go, string -> sgf -> string is tested
// we just test the end status
// TODO: do we have a save or tostring function?
TEST(SgfTest, testMakeSgf) {
  Sgf sgf;
  std::string sgfSample = "";
  sgfSample += "(;CA[UTF-8]SZ[9]PB[Murakawa Daisuke]";
  sgfSample += "PW[Iyama Yuta]KM[6.5]HA[0]RE[W+1.5]GM[1];";
  sgfSample += "B[fd];W[cf];B[eg];W[dd];B[dc];W[cc];B[de];";
  sgfSample += "W[cd];B[ed];W[he];B[ce];W[be];B[df];W[bf];";
  sgfSample += "B[hd];W[ge];B[gd];W[gg];B[db];W[cb];B[cg];";
  sgfSample += "W[bg];B[gh];W[fh];B[hh];W[fg];B[eh];W[ei];";
  sgfSample += "B[di];W[fi];B[hg];W[dh];B[ch];W[ci];B[bh];";
  sgfSample += "W[ff];B[fe];W[hf];B[id];W[bi];B[ah];W[ef];";
  sgfSample += "B[dg];W[ee];B[di];W[ig];B[ai];W[ih];B[fb];";
  sgfSample += "W[hi];B[ag];W[ab];B[bd];W[bc];B[ae];W[ad];";
  sgfSample += "B[af];W[bd];B[ca];W[ba];B[da];W[ie])";

  sgf.load("", sgfSample);
  auto iter = sgf.begin();

  GoState b;
  while (!iter.done()) {
    auto curr = iter.getCurrMove();
    EXPECT_TRUE(b.forward(curr.move));
    ++iter;
  }
}

TEST(SgfTest, testSgfProps) {
  Sgf sgf;
  std::string sgfSample = "";
  sgfSample += "(;GM[1]FF[4]CA[UTF-8]AP[CGoban:3]";
  sgfSample += "ST[2]RU[Chinese]SZ[9]HA[2]RE[Void]KM[5.50]";
  sgfSample += "PW[test_white]PB[test_black]RE[B+39.50];";
  sgfSample += "B[gc];B[cg];W[ee];B[gg];W[eg];B[ge];W[ce];B[ec];";
  sgfSample += "W[cc];B[dd];W[de];B[cd];W[bd];B[bc];W[bb];B[be];";
  sgfSample += "W[ac];B[bf];W[dh];B[ch];W[ci];B[bi];W[di];";
  sgfSample += "B[ah];W[gh];B[hh];W[fh];B[hg];W[gi];B[fg];";
  sgfSample += "W[dg];B[ei];W[cf];B[ef];W[ff];B[fe];W[bg];";
  sgfSample += "B[bh];W[af];B[ag];W[ae];B[ad];W[ae];B[ed];";
  sgfSample += "W[db];B[df];W[eb];B[fb];W[ea];B[fa])";

  sgf.load("", sgfSample);
  auto iter = sgf.begin();

  GoState b;
  while (!iter.done()) {
    auto curr = iter.getCurrMove();
    // to handle handicap
    if (getTurn(b) != curr.player)
      b.forward(0);
    EXPECT_TRUE(b.forward(curr.move));
    ++iter;
  }

  EXPECT_EQ(sgf.getHeader().komi, 5.5);
}

// TODO: our test will fail in case of japanese rule
// will fix it later
TEST(SgfTest, testJapaneseHandicap) {
  Sgf sgf;
  std::string sgfSample = "";
  sgfSample += "(;GM[1]FF[4]CA[UTF-8]AP[CGoban:3]ST[2]RU";
  sgfSample += "[Japanese]SZ[9]HA[2]RE[Void]KM[5.50]PW";
  sgfSample += "[test_white]PB[test_black]AB[gc][cg];W[ee];B[dg])";

  sgf.load("", sgfSample);
  auto iter = sgf.begin();

  GoState b;
  while (!iter.done()) {
    auto curr = iter.getCurrMove();
    // to handle handicap
    if (getTurn(b) != curr.player)
      b.forward(0);
    EXPECT_TRUE(b.forward(curr.move));
    ++iter;
  }
}

TEST(SgfTest, testChineseHandicap) {
  Sgf sgf;
  std::string sgfSample = "";
  sgfSample += "(;GM[1]FF[4]CA[UTF-8]AP[CGoban:3]ST[2]RU";
  sgfSample += "[Chinese]SZ[9]HA[2]RE[Void]KM[5.50]PW";
  sgfSample += "[test_white]PB[test_black]RE[B+39.50];";
  sgfSample += "B[gc];B[cg];W[ee];B[gg];W[eg];B[ge];W[ce];";
  sgfSample += "B[ec];W[cc];B[dd];W[de];B[cd];W[bd];B[bc];";
  sgfSample += "W[bb];B[be];W[ac];B[bf];W[dh];B[ch];W[ci];";
  sgfSample += "B[bi];W[di];B[ah];W[gh];B[hh];W[fh];B[hg];";
  sgfSample += "W[gi];B[fg];W[dg];B[ei];W[cf];B[ef];W[ff];";
  sgfSample += "B[fe];W[bg];B[bh];W[af];B[ag];W[ae];B[ad];";
  sgfSample += "W[ae];B[ed];W[db];B[df];W[eb];B[fb];W[ea];B[fa])";

  sgf.load("", sgfSample);
  auto iter = sgf.begin();

  GoState b;
  while (!iter.done()) {
    auto curr = iter.getCurrMove();
    // to handle handicap
    if (getTurn(b) != curr.player)
      b.forward(0);
    EXPECT_TRUE(b.forward(curr.move));
    ++iter;
  }

  // miracle final board
  std::string finalStr;
  finalStr += "....OX...";
  finalStr += ".O.OOX...";
  finalStr += "O.O.X.X..";
  finalStr += ".OXXX....";
  finalStr += "OX...XX..";
  finalStr += ".X.XXO...";
  finalStr += "X.XOOXXX.";
  finalStr += "XXXO.OOX.";
  finalStr += ".XOOX.O..";
  GoState final_b;
  loadBoard(final_b, finalStr);
  boardEqual(b, final_b);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
