#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "test_elf.h"
#include "elf/options/OptionSpec.h"
#include "elf/options/reflection_option.h"

namespace py = pybind11;

void toOptionsSpec(elf::options::OptionSpec& spec) {
  elf::options::Visitor<elf::Options> visitor("", spec);
}

elf::Options fromOptionsSpec(const elf::options::OptionSpec& spec) {
  elf::Options options;
  elf::options::Saver<elf::Options> visitor("", spec, options);
  return options;
}

PYBIND11_MODULE(_elfgames_tutorial, m) {
  m.def("toOptionsSpec", toOptionsSpec);
  m.def("fromOptionsSpec", fromOptionsSpec);

  py::class_<MyContext>(m, "MyContext")
    .def(py::init<const std::string&>())
    .def("setGameContext", &MyContext::setGameContext);
}
