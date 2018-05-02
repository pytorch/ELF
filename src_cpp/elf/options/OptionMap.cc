/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "OptionMap.h"

#include <stdexcept>

using nlohmann::json;

namespace elf {
namespace options {

void OptionMap::registerPy(pybind11::module& m) {
  namespace py = pybind11;

  py::class_<OptionMap>(m, "OptionMap")
      .def(py::init<OptionSpec>())
      .def(py::init<const OptionMap&>())
      .def("getOptionSpec", &OptionMap::getOptionSpec)
      .def("getJSONString", &OptionMap::getJSONString)
      .def("loadJSONString", &OptionMap::loadJSONString)
      .def("getAsJSONString", &OptionMap::getAsJSONString)
      .def("setAsJSONString", &OptionMap::setAsJSONString);
}

void OptionMap::loadJSON(const json& data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    data_[it.key()] = it.value();
  }
}

const json& OptionMap::getAsJSON(const std::string& optionName) const {
  const auto& elem = data_.find(optionName);
  if (elem == data_.end()) {
    throw std::runtime_error(optionName + " has not been set!");
  }
  return elem.value();
}

} // namespace options
} // namespace elf
