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

#include <chrono>

TEST(SpeedTest, testSpeed) {
  using namespace std::chrono;

  GoState b;
  std::mt19937 rng(time(NULL));
  const int kTrial = 10000;
  const int kMove = 300;

  duration<double> dur(0);

  int move_count = 0;
  for (int i = 0; i < kTrial; i++) {
    b.reset();

    for (int j = 0; j < kMove; j++) {
      const std::vector<Coord> moves = b.getAllValidMoves();
      if (moves.size() == 0) break;

      Coord curr = moves[rng() % moves.size()];
      auto now = high_resolution_clock::now();
      b.forward(curr);
      dur += duration_cast<duration<double>>(high_resolution_clock::now() - now); 
      move_count ++;
    }
  }

  std::cout << "#Moves: " << move_count << " Time spent per move: " << dur.count() * 10e6 / move_count << " microseconds" << std::endl;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
