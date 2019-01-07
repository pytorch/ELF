#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/functional.h>

#include "elf/options/pybind_utils.h"
#include "snippets.h"

namespace elf {

namespace snippet {

namespace py = pybind11;

void reg_pybind11(py::module &m) {
  options::PyInterface<Options>(m, "Options");

  auto ref = py::return_value_policy::reference_internal;

  py::class_<Interface>(m, "_Interface");

  py::class_<MyContext>(m, "MyContext")
    .def(py::init<const Options &, const std::string&, const std::string &>())
    .def("setGameContext", &MyContext::setGameContext)
    .def("setInterface", &MyContext::setInterface, ref)
    .def("getParams", &MyContext::getParams)
    .def("getSummary", &MyContext::getSummary)
    .def("getBatchSpec", &MyContext::getBatchSpec);
}

}  // namespace snippet

}  // namespace elf
