#pragma once

#include <pybind11/pybind11.h>
#include "OptionSpec.h"
#include "reflection_option.h"

namespace elf {

namespace options {

template <typename Options>
class PyInterface {
 public:
  static void setSpec(OptionSpec& spec) {
    Visitor<Options> visitor("", spec);
  }

  static Options getArgs(const OptionSpec& spec) {
    Options options;
    Saver<Options> visitor("", spec, options);
    return options;
  }

  PyInterface(pybind11::module &m, std::string name, bool init_class = true) {
    if (init_class) {
      pybind11::class_<Options>(m, name.c_str())
        .def(pybind11::init<>());
    }
    m.def(("setSpec" + name).c_str(), setSpec);
    m.def(("getArgs" + name).c_str(), getArgs);
  }
};

}  // namespace options

}  // namespace elf

