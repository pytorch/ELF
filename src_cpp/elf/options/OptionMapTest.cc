/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "OptionMap.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "OptionSpecTest.h"

namespace elf {
namespace options {

class OptionMapTest : public OptionSpecTest {
 protected:
  virtual void SetUp() {
    OptionSpecTest::SetUp();

    emptyMap_ = std::make_unique<OptionMap>(mergedSpec_);
    fullMap_ = std::make_unique<OptionMap>(*emptyMap_);
    fullMap_->set("some_int", 3);
    fullMap_->set<std::string>("some_str", "hello");
    fullMap_->set<std::vector<double>>("some_floats", {1.2, 3.4});
    fullMap_->set("some_bool", false);
    fullMap_->set("some_other_bool", true);
    data_ = {{"some_int", 3},
             {"some_str", "hello"},
             {"some_floats", {1.2, 3.4}},
             {"some_bool", false},
             {"some_other_bool", true}};
  }

  std::unique_ptr<OptionMap> emptyMap_, fullMap_;
  nlohmann::json data_;
};

TEST_F(OptionMapTest, getOptionSpec) {
  auto mergedNames = mergedSpec_.getOptionNames();
  auto emptyNames = emptyMap_->getOptionSpec().getOptionNames();
  auto fullNames = fullMap_->getOptionSpec().getOptionNames();
  std::sort(mergedNames.begin(), mergedNames.end());
  std::sort(emptyNames.begin(), emptyNames.end());
  std::sort(fullNames.begin(), fullNames.end());

  EXPECT_EQ(mergedNames, emptyNames);
  EXPECT_EQ(mergedNames, fullNames);
}

TEST_F(OptionMapTest, get) {
  EXPECT_EQ(fullMap_->get<int>("some_int"), data_["some_int"]);
  EXPECT_EQ(fullMap_->get<std::string>("some_str"), data_["some_str"]);
  EXPECT_EQ(
      fullMap_->get<std::vector<double>>("some_floats"), data_["some_floats"]);
  EXPECT_EQ(fullMap_->get<bool>("some_bool"), data_["some_bool"]);
  EXPECT_EQ(fullMap_->get<bool>("some_other_bool"), data_["some_other_bool"]);
}

TEST_F(OptionMapTest, loadJSON_and_getJSON) {
  OptionMap loadedMap = *emptyMap_;
  loadedMap.loadJSON(data_);

  EXPECT_EQ(data_, loadedMap.getJSON());
  EXPECT_EQ(data_, fullMap_->getJSON());
}

TEST_F(OptionMapTest, setAsJSON_and_getAsJSON) {
  OptionMap loadedMap = *emptyMap_;
  for (auto it = data_.begin(); it != data_.end(); ++it) {
    loadedMap.setAsJSON(it.key(), it.value());
  }

  for (auto it = data_.begin(); it != data_.end(); ++it) {
    EXPECT_EQ(it.value(), loadedMap.getAsJSON(it.key()));
  }
}

} // namespace options
} // namespace elf

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
