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
#include <vector>

#include "elfgames/go/base/board_feature.h"
#include "elfgames/go/base/go_state.h"
#include "test_utils.h"

static constexpr size_t kBoardRegion = BOARD_SIZE * BOARD_SIZE;

TEST(FeatureTest, testAgzFeature) {
  GoState s;

  for (auto c :
       {toFlat(0, 0), toFlat(0, 1), toFlat(0, 2), toFlat(0, 3), toFlat(1, 1)})
    s.forward(c);

  const BoardFeature& bf(s);
  std::vector<float> features;
  // to extract a certain channel
  std::vector<float> subFeatures;
  bf.extractAGZ(&features);

  // check shape, we use 18 instead of 17 in AGZ
  // 8 recent * 2 + 2 player = 18
  EXPECT_EQ(features.size(), kBoardRegion * 18);

  std::vector<float> featureGt(kBoardRegion, 0.);

  // check channel-0
  featureGt[3] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin(), features.begin() + kBoardRegion, featureGt.begin()));

  // check channel-1
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  featureGt[0] = 1.;
  featureGt[2] = 1.;
  featureGt[10] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin() + kBoardRegion,
      features.begin() + 2 * kBoardRegion,
      featureGt.begin()));

  // check channel-2
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  featureGt[1] = 1.;
  featureGt[3] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin() + 2 * kBoardRegion,
      features.begin() + 3 * kBoardRegion,
      featureGt.begin()));

  // check channel-3
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  featureGt[0] = 1.;
  featureGt[2] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin() + 3 * kBoardRegion,
      features.begin() + 4 * kBoardRegion,
      featureGt.begin()));

  // check channel-4
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  featureGt[1] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin() + 4 * kBoardRegion,
      features.begin() + 5 * kBoardRegion,
      featureGt.begin()));

  // check channel-5
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  featureGt[0] = 1.;
  featureGt[2] = 1.;
  EXPECT_TRUE(std::equal(
      features.begin() + 5 * kBoardRegion,
      features.begin() + 6 * kBoardRegion,
      featureGt.begin()));

  // check channel 10-15
  std::fill(featureGt.begin(), featureGt.end(), 0.);
  for (int i = 10; i < 16; ++i) {
    EXPECT_TRUE(std::equal(
        features.begin() + i * kBoardRegion,
        features.begin() + (i + 1) * kBoardRegion,
        featureGt.begin()));
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
