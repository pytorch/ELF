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
#include <array>
#include <vector>

#include "elfgames/go/base/board_feature.h"
#include "elfgames/go/base/go_state.h"
#include "test_utils.h"

static constexpr size_t kBoardRegion = BOARD_SIZE * BOARD_SIZE;

float* boardPlane(float* features, int idx) {
  return features + idx * BOARD_SIZE * BOARD_SIZE;
}

const float* boardPlane(const float* features, int idx) {
  return features + idx * BOARD_SIZE * BOARD_SIZE;
}

bool featEqual(const float* feat1, const float* feat2, size_t N) {
  // assert(feat1.size() == feat2.size());
  for (size_t i = 0; i < N; ++i) {
    if (fabs(feat1[i] - feat2[i]) > 1e-3)
      return false;
  }
  return true;
}

void InvTransform(const BoardFeature& bf, const float* feat, float* featBack) {
  // for channel i
  for (int i = 0; i < 18; ++i) {
    const float* planeFeat = boardPlane(feat, i);
    float* planeFeatBack = boardPlane(featBack, i);

    // inv-transform
    for (int x = 0; x < BOARD_SIZE; ++x) {
      for (int y = 0; y < BOARD_SIZE; ++y) {
        // get xy as a std::pair<int, int>
        auto xy = bf.InvTransform(std::make_pair(x, y));
        int offset = EXPORT_OFFSET_XY(x, y);
        int offsetBack = EXPORT_OFFSET_XY(xy.first, xy.second);
        planeFeatBack[offsetBack] = planeFeat[offset];
      }
    }
  }
}

// in case we want to print the feature for further debug
void printFeat(float* data) {
  for (int i = 0; i < 3; ++i) {
    float* planeData = boardPlane(data, i);
    std::cout << "channel " << i << ": " << std::endl;
    for (size_t j = 0; j < kBoardRegion; ++j) {
      std::cout << planeData[j] << " ";
      if ((j + 1) % BOARD_SIZE == 0)
        std::cout << std::endl;
    }
    std::cout << std::endl;
  }
}

TEST(SymmetryTest, testInversions) {
  std::mt19937 rng;
  GoState s;
  // play some random moves
  s.forward(toFlat(0, 8));
  s.forward(toFlat(1, 7));

  BoardFeature bf(s);
  BoardFeature randomBf = BoardFeature::RandomShuffle(s, &rng);

  std::array<float, kBoardRegion * 18> agzFeat;
  std::array<float, kBoardRegion * 18> agzFeatSymm;
  std::array<float, kBoardRegion * 18> agzFeatBack;

  auto player1 = bf.state().nextPlayer();
  auto player2 = randomBf.state().nextPlayer();
  EXPECT_EQ(player1, player2);

  for (int i = 0; i < 18; ++i) {
    // directly extract
    bf.extractAGZ(agzFeat.data());

    // extract feature with symmetry_type = i
    randomBf.setD4Code(i);
    randomBf.extractAGZ(agzFeatSymm.data());
    // apply the symmetry back
    InvTransform(randomBf, agzFeatSymm.data(), agzFeatBack.data());

    // check still equal after transformed back
    for (size_t j = 0; j < kBoardRegion * 18; ++j) {
      EXPECT_FLOAT_EQ(agzFeat[j], agzFeatBack[j]);
    }
  }
}

// We didn't extract the feature first and then
// apply symmetry on it (different from minigo)
// so we test on the coordinate
TEST(SymmetryTest, testCompositions) {
  std::mt19937 rng1;
  std::mt19937 rng2;
  std::mt19937 rng3;
  GoState s;

  BoardFeature randomBf1 = BoardFeature::RandomShuffle(s, &rng1);
  BoardFeature randomBf2 = BoardFeature::RandomShuffle(s, &rng2);
  BoardFeature randomBf3 = BoardFeature::RandomShuffle(s, &rng3);

  randomBf1.setD4Group(BoardFeature::CCW90, false);
  randomBf2.setD4Group(BoardFeature::CCW180, false);
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      auto pair1 = randomBf1.Transform(std::make_pair(x, y));
      auto pair2 = randomBf1.Transform(pair1);
      auto pair3 = randomBf2.Transform(std::make_pair(x, y));
      EXPECT_EQ(pair2.first, pair3.first);
      EXPECT_EQ(pair2.second, pair3.second);
    }
  }

  randomBf1.setD4Group(BoardFeature::CCW90, false);
  randomBf2.setD4Group(BoardFeature::CCW180, false);
  randomBf3.setD4Group(BoardFeature::CCW270, false);
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      auto pair1 = randomBf1.Transform(std::make_pair(x, y));
      auto pair2 = randomBf2.Transform(pair1);
      auto pair3 = randomBf3.Transform(std::make_pair(x, y));
      EXPECT_EQ(pair2.first, pair3.first);
      EXPECT_EQ(pair2.second, pair3.second);
    }
  }

  randomBf1.setD4Group(BoardFeature::NONE, false);
  randomBf2.setD4Group(BoardFeature::CCW90, false);
  randomBf3.setD4Group(BoardFeature::CCW90, false);
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      auto pair1 = randomBf1.Transform(std::make_pair(x, y));
      auto pair2 = randomBf2.Transform(pair1);
      auto pair3 = randomBf3.Transform(std::make_pair(x, y));
      EXPECT_EQ(pair2.first, pair3.first);
      EXPECT_EQ(pair2.second, pair3.second);
    }
  }

  randomBf1.setD4Group(BoardFeature::CCW90, true);
  randomBf2.setD4Group(BoardFeature::CCW90, false);
  randomBf3.setD4Group(BoardFeature::NONE, true);
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      // x, y -> N - x, y
      auto pair1 = randomBf1.Transform(std::make_pair(x, y));
      // N-x, y -> y, x
      auto pair2 = randomBf2.Transform(pair1);
      // x, y -> y, x
      auto pair3 = randomBf3.Transform(std::make_pair(x, y));
      EXPECT_EQ(pair2.first, pair3.first); //, BOARD_SIZE - 1);
      EXPECT_EQ(pair2.second, pair3.second); //, BOARD_SIZE - 1);
    }
  }

  randomBf1.setD4Group(BoardFeature::CCW90, false);
  randomBf2.setD4Group(BoardFeature::CCW270, false);
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      // x, y -> y, N - x
      auto pair1 = randomBf1.Transform(std::make_pair(x, y));
      // y, N-x -> x, y
      auto pair2 = randomBf2.Transform(pair1);
      // x, y -> y, x
      // auto pair3 = random_bf3.Transform(std::make_pair(x, y));
      /*
      std::cout << "original: " << x << y << std::endl;
      std::cout << "pair1: " << pair1.first << pair1.second << std::endl;
      std::cout << "pair2: " << pair2.first << pair2.second << std::endl;
      std::cout << "pair3: " << pair3.first << pair3.second << std::endl;
      */
      EXPECT_EQ(pair2.first, x); //, BOARD_SIZE - 1);
      EXPECT_EQ(pair2.second, y); //, BOARD_SIZE - 1);
    }
  }
}

// Test different symmetries generates
// different results on both feature and probability
TEST(SymmetryTest, testUniqueness) {
  std::mt19937 rng1;
  std::mt19937 rng2;
  GoState s;
  // play some random moves
  s.forward(toFlat(0, 8));
  s.forward(toFlat(1, 5));

  BoardFeature randomBf1 = BoardFeature::RandomShuffle(s, &rng1);
  BoardFeature randomBf2 = BoardFeature::RandomShuffle(s, &rng2);

  std::array<float, kBoardRegion * 18> agzFeat1;
  std::array<float, kBoardRegion * 18> agzFeat2;

  for (int i = 0; i < 8; ++i) {
    randomBf1.setD4Code(i);
    randomBf1.extractAGZ(agzFeat1.data());

    for (int j = i + 1; j < 8; ++j) {
      randomBf2.setD4Code(j);
      randomBf2.extractAGZ(agzFeat2.data());

      // expect agz1 and agz2 not equal
      EXPECT_FALSE(
          featEqual(agzFeat1.data(), agzFeat2.data(), kBoardRegion * 18));
    }
  }

  /* simulate applying random symmetry on output pi:
    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution(0.0,1.0);
    std::vector<float> pi(0., kBoardRegion + 1);
    for (size_t i = 0; i < pi.size(); ++i)
      pi[i] = distribution(generator);
    std::unordered_map<int, float> output_pi1;
    std::unordered_map<int, float> output_pi2;
  */
  for (int i = 0; i < 8; ++i) {
    // output_pi1.clear();
    randomBf1.setD4Code(i);
    for (int j = i + 1; j < 8; ++j) {
      // output_pi2.clear();
      randomBf2.setD4Code(j);

      // check at least one coord not equal
      bool flag = true;
      for (size_t k = 0; k < kBoardRegion + 1; ++k) {
        Coord m1 = randomBf1.action2Coord(k);
        Coord m2 = randomBf2.action2Coord(k);
        if (m1 != m2) {
          flag = false;
          break;
        }
      }
      EXPECT_FALSE(flag);
    }
  }
}

// Check that the reinterpretation of
// kBoardRegion = BOARD_SIZE ^ 2 + 1 during symmetry
// application is consistent with coords.from_flat
TEST(SymmetryTest, testProperMoveTransform) {
  std::mt19937 rng;
  GoState s;

  BoardFeature randomBf = BoardFeature::RandomShuffle(s, &rng);

  for (int i = 0; i < 8; ++i) {
    randomBf.setD4Code(i);
    for (size_t i = 0; i < kBoardRegion; ++i) {
      Coord m = randomBf.action2Coord(i);
      // 0: aa; 1: ab; 2: ac

      int x = i / BOARD_SIZE;
      int y = i % BOARD_SIZE;
      // int offset = random_bf.Transform(x, y);
      auto p = randomBf.InvTransform(std::make_pair(x, y));
      EXPECT_EQ(p.first, X(m));
      EXPECT_EQ(p.second, Y(m));
    }
    EXPECT_EQ(randomBf.action2Coord(kBoardRegion), 0);
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
