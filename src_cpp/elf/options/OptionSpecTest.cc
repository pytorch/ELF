/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "OptionSpecTest.h"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace elf {
namespace options {

TEST_F(OptionSpecTest, getOptionNames) {
  auto names = spec1_.getOptionNames();
  std::sort(names.begin(), names.end());

  const std::vector<std::string> expectedNames = {
      "some_floats", "some_int", "some_str"};
  EXPECT_EQ(expectedNames, names);
}

TEST_F(OptionSpecTest, merge) {
  auto names = mergedSpec_.getOptionNames();
  std::sort(names.begin(), names.end());

  const std::vector<std::string> expectedNames = {
      "some_bool", "some_floats", "some_int", "some_other_bool", "some_str"};
  EXPECT_EQ(expectedNames, names);
}

TEST_F(OptionSpecTest, getPythonArgparseOptionsAsJSON) {
  auto options = mergedSpec_.getPythonArgparseOptionsAsJSON();
  sort(options.begin(), options.end(), [](const auto& o1, const auto& o2) {
    return o1["args"][0] < o2["args"][0];
  });

  /* clang-format off */
  const nlohmann::json expectedOptions = {
    {
      {"args", {"--no_some_other_bool"}},
      {"kwargs", {
        {"action", "store_false"},
        {"dest", "some_other_bool"},
        {"help", "some_other_bool help"}
      }}
    },
    {
      {"args", {"--some_bool"}},
      {"kwargs", {
        {"action", "store_true"},
        {"dest", "some_bool"},
        {"help", "some_bool help"}
      }}
    },
    {
      {"args", {"--some_floats"}},
      {"kwargs", {
        {"default", {1.23, 4.56}},
        {"dest", "some_floats"},
        {"help", "some_floats help"},
        {"nargs", "*"},
        {"required", false},
        {"type", "float"}
      }}
    },
    {
      {"args", {"--some_int"}},
      {"kwargs", {
        {"dest", "some_int"},
        {"help", "some_int help"},
        {"required", true},
        {"type", "int"}
      }}
    },
    {
      {"args", {"--some_str"}},
      {"kwargs", {
        {"default", "a default"},
        {"dest", "some_str"},
        {"help", "some_str help"},
        {"required", false},
        {"type", "str"}
      }}
    }
  };
  /* clang-format on */
  EXPECT_EQ(expectedOptions, options);
}

} // namespace options
} // namespace elf

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
