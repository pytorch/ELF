/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "Pybind.h"

#include "IndexedLoggerFactory.h"
#include "Levels.h"

namespace elf {
namespace logging {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  using spdlog::logger;

  IndexedLoggerFactory::registerPy(m);
  Levels::registerPy(m);

/* clang-format off*/
// disable formatting due to macros

// This macro defines a logging function for the given loglevel in Pythonland.
#define _ELF_PYBIND_DECLARE_LOG_LEVEL(level)             \
  .def(                                                  \
      #level,                                            \
      static_cast<void (logger::*)(const std::string&)>( \
          &logger::level<std::string>))

  py::class_<logger, std::shared_ptr<logger>>(m, "Logger")
      _ELF_PYBIND_DECLARE_LOG_LEVEL(trace) _ELF_PYBIND_DECLARE_LOG_LEVEL(debug)
          _ELF_PYBIND_DECLARE_LOG_LEVEL(info)
              _ELF_PYBIND_DECLARE_LOG_LEVEL(warn)
                  _ELF_PYBIND_DECLARE_LOG_LEVEL(error)
                      _ELF_PYBIND_DECLARE_LOG_LEVEL(critical)
                          .def("flush", &logger::flush)
                          .def("flush_on", &logger::flush_on)
                          .def("level", &logger::level)
                          .def("name", &logger::name)
                          .def("set_formatter", &logger::set_formatter)
                          .def("set_level", &logger::set_level)
                          .def("should_log", &logger::should_log);

#undef _ELF_PYBIND_DECLARE_LOG_LEVEL

  m.def("drop", spdlog::drop)
      .def("drop_all", spdlog::drop_all)
      .def("get", spdlog::get)
      .def("set_level", spdlog::set_level)
      .def("set_pattern", spdlog::set_pattern)
      .def("daily_logger_mt", spdlog::daily_logger_mt)
      .def("rotating_logger_mt", spdlog::rotating_logger_mt)
      .def("stdout_logger_mt", spdlog::stdout_logger_mt)
      .def("stderr_logger_mt", spdlog::stderr_logger_mt)
      .def("stdout_color_mt", spdlog::stdout_color_mt)
      .def("stderr_color_mt", spdlog::stderr_color_mt);
  /* clang-format on */
}

} // namespace logging
} // namespace elf
