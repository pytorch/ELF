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
#include "elfgames/go/sgf/sgf.h"

// All tests assumes BOARD_SIZE = 9
// We use padding=1, so actually BOARD_EXPAND_SIZE = 11
// For a coordinate (x, y)
// flatten is (x+1) + (y+1)*BOARD_EXPAND_SIZE
TEST(CoordTest, testUpperLeft) {
  // We use the pygtp system for coordinates
  Coord c = str2coord("aa");
  EXPECT_EQ(c, 12);
  std::string str = coord2str(c);

  // TEST: aa -> c -> aa
  EXPECT_EQ(str, "aa");
  int x = X(c);
  int y = Y(c);
  EXPECT_EQ(x, 0);
  EXPECT_EQ(y, 0);
}

TEST(CoordTest, testTopLeft) {
  // We use the pygtp system for coordinates
  Coord c = str2coord("ia");
  EXPECT_EQ(c, 20);
  std::string str = coord2str(c);

  // TEST: ia -> c -> ia
  EXPECT_EQ(str, "ia");
  int x = X(c);
  int y = Y(c);
  EXPECT_EQ(x, 8);
  EXPECT_EQ(y, 0);
}

TEST(CoordTest, testPass) {
  Coord c = str2coord("");
  EXPECT_EQ(c, 0);
  std::string str = coord2str(c);
  EXPECT_EQ(str, "");
}

TEST(CoordTest, testParse9x9) {
  int x, y;
  Coord c;
  std::string str;

  c = str2coord("aa");
  x = X(c);
  y = Y(c);
  EXPECT_EQ(x, 0);
  EXPECT_EQ(y, 0);

  c = str2coord("ac");
  x = X(c);
  y = Y(c);
  EXPECT_EQ(x, 0);
  EXPECT_EQ(y, 2);

  c = str2coord("ca");
  x = X(c);
  y = Y(c);
  EXPECT_EQ(x, 2);
  EXPECT_EQ(y, 0);

  str = coord2str(str2coord("aa"));
  EXPECT_EQ(str, "aa");

  str = coord2str(str2coord("ha"));
  EXPECT_EQ(str, "ha");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
