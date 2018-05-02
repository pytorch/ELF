/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "Levels.h"

#include <algorithm>

namespace elf {
namespace logging {

void Levels::registerPy(pybind11::module& m) {
  namespace py = pybind11;
  using spdlog::level::level_enum;

  py::enum_<level_enum>(m, "LoggerLevel")
      .value("trace", level_enum::trace)
      .value("debug", level_enum::debug)
      .value("info", level_enum::info)
      .value("warn", level_enum::warn)
      .value("err", level_enum::err)
      .value("critical", level_enum::critical)
      .value("off", level_enum::off)
      .value("invalid", static_cast<level_enum>(Levels::INVALID))
      .def_static(
          "from_str",
          static_cast<level_enum (*)(const char*)>(&Levels::from_str));
}

spdlog::level::level_enum Levels::from_str(const char* str) {
  using spdlog::level::level_names;
  constexpr size_t num_levels = sizeof(level_names) / sizeof(level_names[0]);
  const char** begin = level_names;
  const char** end = level_names + num_levels;
  const char** iter = std::find_if(begin, end, [=](const char* level_name) {
    return strcmp(level_name, str) == 0;
  });
  if (iter == end) {
    return static_cast<spdlog::level::level_enum>(INVALID);
  }
  return static_cast<spdlog::level::level_enum>(iter - begin);
}

} // namespace logging
} // namespace elf
