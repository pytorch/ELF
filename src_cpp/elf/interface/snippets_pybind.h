#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/functional.h>

#include "snippets.h"

namespace elf {

namespace snippet {

namespace py = pybind11;

void reg_pybind11(py::module &m) {
  py::class_<Options>(m, "Options")
    .def(py::init<>());

  py::class_<GameFactory>(m, "GameFactory");

  py::class_<MyContext>(m, "MyContext")
    .def(py::init<const Options &, const std::string&, const std::string &>())
    .def("setGameContext", &MyContext::setGameContext)
    .def("setGameFactory", &MyContext::setGameFactory)
    .def("getParams", &MyContext::getParams)
    .def("getBatchSpec", &MyContext::getBatchSpec);
}

}  // namespace snippet

}  // namespace elf
