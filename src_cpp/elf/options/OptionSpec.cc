/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "OptionSpec.h"

namespace elf {
namespace options {

void OptionSpec::registerPy(pybind11::module& m) {
  /* clang-format off */
  namespace py = pybind11;

  /**
   * This macro defines four Add*Option methods for OptionSpec in Pythonland.
   *
   * - Required
   * - Optional with default
   * - List (required)
   * - List (optional with default)
   *
   * T is the C++ type, and
   * name is the name of the type in the Python function name.
   *
   * e.g. the first invocation of the macro below declares addIntOption() and
   * addIntListOption(), each with required (i.e. no default) and optional
   * (i.e. with default) versions.
   */
  #define _ELF_PYBIND_DECLARE_ADD_OPTION(T, name) \
    def( \
        "add" #name "Option", \
        static_cast<bool (OptionSpec::*)(std::string, std::string)>( \
            &OptionSpec::addOption<T>)) \
    .def( \
        "add" #name "Option", \
        static_cast<bool (OptionSpec::*)(std::string, std::string, T)>( \
            &OptionSpec::addOption<T>)) \
    .def( \
        "add" #name "ListOption", \
        static_cast<bool (OptionSpec::*)(std::string, std::string)>( \
            &OptionSpec::addOption<std::vector<T>>)) \
    .def( \
        "add" #name "ListOption", \
        static_cast<bool (OptionSpec::*)(std::string, std::string, std::vector<T>)>( \
            &OptionSpec::addOption<std::vector<T>>))
  /* clang-format on */

  py::class_<OptionSpec>(m, "OptionSpec")
      ._ELF_PYBIND_DECLARE_ADD_OPTION(int32_t, Int)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(int32_t, Int32)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(uint32_t, UnsignedInt32)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(int64_t, Int64)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(uint64_t, UnsignedInt64)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(double, Float)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(double, Float64)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(float, Float32)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(bool, Bool)
      ._ELF_PYBIND_DECLARE_ADD_OPTION(std::string, Str)
      .def(py::init<>())
      .def(py::init<const OptionSpec&>())
      .def("getOptionNames", &OptionSpec::getOptionNames)
      .def(
          "getPythonArgparseOptionsAsJSONString",
          &OptionSpec::getPythonArgparseOptionsAsJSONString)
      .def("merge", &OptionSpec::merge)
      .def(
          "addPrefixSuffixToOptionNames",
          &OptionSpec::addPrefixSuffixToOptionNames);
}

} // namespace options
} // namespace elf
