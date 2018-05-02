/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <pybind11/pybind11.h>

#include <string>

#include <spdlog/spdlog.h>

namespace elf {
namespace logging {

class Levels {
 public:
  static constexpr int INVALID = 127; // hack, but guaranteed to fit any enum

  static void registerPy(pybind11::module& m);

  static spdlog::level::level_enum from_str(const char* str);

  static spdlog::level::level_enum from_str(const std::string& str) {
    return from_str(str.c_str());
  }
};

} // namespace logging
} // namespace elf
