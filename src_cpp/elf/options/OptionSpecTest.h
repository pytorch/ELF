/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "OptionSpec.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace elf {
namespace options {

class OptionSpecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    spec1_.addOption<int>("some_int", "some_int help");
    spec1_.addOption<std::string>("some_str", "some_str help", "a default");
    spec1_.addOption<std::vector<double>>(
        "some_floats", "some_floats help", {1.23, 4.56});
    spec2_.addOption<bool>("some_bool", "some_bool help", false);
    spec2_.addOption<bool>("some_other_bool", "some_other_bool help", true);
    mergedSpec_ = spec1_;
    mergedSpec_.merge(spec2_);
  }

  OptionSpec spec1_, spec2_, mergedSpec_;
};

} // namespace options
} // namespace elf
