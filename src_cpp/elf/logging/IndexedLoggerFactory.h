/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * IndexedLoggerFactory is a class to create multiple spdlog::loggers, each
 * indexed by a strictly increasing counter.
 *
 * The typical usecase is when you want to associate one logger per class
 * instance. In this case, you'd do something like:
 *
 * namespace A {
 *
 * class B {
 *  public:
 *   B(std::string description)
 *       : logger_(getLoggerFactory()->makeLogger("A::B-", "-" + description)),
 *         description_(description) {}
 *
 *  private:
 *   static IndexedLoggerFactory* getLoggerFactory() {
 *     static IndexedLoggerFactory factory([](const std::string& name) {
 *       return spdlog::stderr_color_mt(name);
 *     });
 *     return &factory;
 *   }
 *
 *   std::shared_ptr<spdlog::logger> logger_;
 *   std::string description_;
 * };
 *
 * }
 */

#pragma once

#include <pybind11/pybind11.h>

#include <atomic>
#include <functional>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace elf {
namespace logging {

class IndexedLoggerFactory {
 public:
  using CreatorT =
      std::function<std::shared_ptr<spdlog::logger>(const std::string& name)>;

  static void registerPy(pybind11::module& m);

  IndexedLoggerFactory(CreatorT creator, size_t initIndex = 0)
      : creator_(std::move(creator)), counter_(initIndex) {}

  std::shared_ptr<spdlog::logger> makeLogger(
      const std::string& prefix,
      const std::string& suffix);

 private:
  CreatorT creator_;
  std::atomic_size_t counter_;
};

inline std::shared_ptr<spdlog::logger> getLogger(
    const std::string& prefix,
    const std::string& suffix) {
  static IndexedLoggerFactory factory(
      [](const std::string& name) { return spdlog::stderr_color_mt(name); });
  return factory.makeLogger(prefix, suffix);
}

} // namespace logging
} // namespace elf
