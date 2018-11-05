/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "board_feature.h"
#include <cassert>
#include <cmath>
#include <utility>
#include "go_state.h"

#define S_ISA(c1, c2) ((c2 == S_EMPTY) || (c1 == c2))

void BoardFeature::extract(std::vector<float>* features) const {
  features->resize(TOTAL_FEATURE_SIZE);
  extract(&(*features)[0]);
}

void BoardFeature::extract(float* features) const {
  std::copy(s_.GetFeatures(), s_.GetFeatures() + TOTAL_FEATURE_SIZE, features);   

}

void BoardFeature::extractAGZ(std::vector<float>* features) const {
  features->resize(TOTAL_FEATURE_SIZE);
  extractAGZ(&(*features)[0]);  
}

void BoardFeature::extractAGZ(float* features) const {
  std::fill(features, features + TOTAL_FEATURE_SIZE, 0.0);

}
