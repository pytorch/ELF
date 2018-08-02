/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * The OptionMap class is a container for optionName:value pairs as specified
 * by an OptionSpec object.
 *
 * void loadJSON(data)
 *   loads a JSON object (optionName:value format) into the map
 *
 * nlohmann::json getJSON()
 *   returns the values of the option map as a JSON object (format as above)
 *
 * void set<T>(optionName, value)
 * T get<T>(optionName)
 *   self-explanatory
 *
 * void setAsJSON(optionName, data)
 * nlohmann::json getAsJSON(optionName)
 *   same as set/get, but takes/returns a JSON value instead of raw C++ value
 *
 * const OptionSpec& getOptionSpec()
 *   returns the underlying OptionSpec
 */

#pragma once

#include <pybind11/pybind11.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "OptionSpec.h"

namespace elf {
namespace options {

class OptionMap {
  using json = nlohmann::json;

 public:
  static void registerPy(pybind11::module& m);

  OptionMap(OptionSpec spec) : spec_(std::move(spec)), data_(json::object()) {}

  void loadJSON(const json& data);

  void loadJSONString(const std::string& dataString) {
    loadJSON(json::parse(dataString));
  }

  const json& getJSON() const {
    return data_;
  }

  std::string getJSONString() const {
    return getJSON().dump();
  }

  void setAsJSON(const std::string& optionName, const json& data) {
    data_[optionName] = data;
  }

  void setAsJSONString(
      const std::string& optionName,
      const std::string& dataString) {
    setAsJSON(optionName, json::parse(dataString));
  }

  void set(const std::string& optionName, const json& j) {
    auto& optionInfo = spec_.getOptionInfo(optionName);
    data_[optionName] = j;
    optionInfo.setValueFromJSON(j);
  }

  const json& getAsJSON(const std::string& optionName) const;

  std::string getAsJSONString(const std::string& optionName) const {
    return getAsJSON(optionName).dump();
  }

  template <typename T>
  T get(const std::string& optionName) const {
    const auto& optionInfo = spec_.getOptionInfo(optionName);
    return optionInfo.fromJSON<T>(getAsJSON(optionName));
  }

  const OptionSpec& getOptionSpec() const {
    return spec_;
  }

 private:
  OptionSpec spec_;

  json data_;
};

} // namespace options
} // namespace elf
