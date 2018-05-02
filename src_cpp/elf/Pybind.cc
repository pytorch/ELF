/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "Pybind.h"

#include <stdint.h>

#include <spdlog/spdlog.h>

#include "elf/ai/tree_search/tree_search_options.h"
#include "elf/base/context.h"
#include "elf/comm/comm.h"
#include "elf/logging/Pybind.h"
#include "elf/options/Pybind.h"

namespace {

void register_common_func(pybind11::module& m) {
  namespace py = pybind11;

  using comm::ReplyStatus;
  using elf::AnyP;
  using elf::Context;
  using elf::FuncMapBase;
  using elf::SharedMem;
  using elf::SharedMemOptions;
  using elf::Size;

  auto ref = py::return_value_policy::reference_internal;

  py::enum_<ReplyStatus>(m, "ReplyStatus")
      .value("SUCCESS", ReplyStatus::SUCCESS)
      .value("FAILED", ReplyStatus::FAILED)
      .value("UNKNOWN", ReplyStatus::UNKNOWN)
      .export_values();

  py::class_<Context>(m, "Context")
      .def(
          "wait",
          &Context::wait,
          py::arg("timeout_usec") = 0,
          ref,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "step",
          &Context::step,
          py::arg("success") = comm::SUCCESS,
          py::call_guard<py::gil_scoped_release>())
      .def("start", &Context::start)
      .def("stop", &Context::stop)
      .def("version", &Context::version)
      .def("allocateSharedMem", &Context::allocateSharedMem, ref)
      .def("createSharedMemOptions", &Context::createSharedMemOptions);

  py::class_<Size>(m, "Size").def("vec", &Size::vec, ref);

  py::class_<SharedMemOptions>(m, "SharedMemOptions")
      .def("idx", &SharedMemOptions::getIdx)
      .def("batchsize", &SharedMemOptions::getBatchSize)
      .def("label", &SharedMemOptions::getLabel, ref)
      .def("setTimeout", &SharedMemOptions::setTimeout);

  py::class_<SharedMem>(m, "SharedMem")
      .def("__getitem__", &SharedMem::get, ref)
      .def("getSharedMemOptions", &SharedMem::getSharedMemOptions, ref)
      .def("effective_batchsize", &SharedMem::getEffectiveBatchSize)
      .def("info", &SharedMem::info);

  py::class_<AnyP>(m, "AnyP")
      .def("info", &AnyP::info)
      .def("field", &AnyP::field, ref)
      .def("set", &AnyP::setAddress);

  py::class_<FuncMapBase>(m, "FuncMapBase")
      .def("batchsize", &FuncMapBase::getBatchSize)
      .def("name", &FuncMapBase::getName, ref)
      .def("sz", &FuncMapBase::getSize, ref)
      .def("type_name", &FuncMapBase::getTypeName)
      .def("type_size", &FuncMapBase::getSizeOfType);
}

void register_tree_search(pybind11::module& m) {
  namespace py = pybind11;

  using elf::ai::tree_search::SearchAlgoOptions;
  using elf::ai::tree_search::TSOptions;

  PYCLASS_WITH_FIELDS(m, SearchAlgoOptions).def(py::init<>());
  PYCLASS_WITH_FIELDS(m, TSOptions).def(py::init<>());
}

} // namespace

namespace elf {

void registerPy(pybind11::module& m) {
  register_common_func(m);

  auto m_logging = m.def_submodule("_logging");
  elf::logging::registerPy(m_logging);

  auto m_options = m.def_submodule("_options");
  elf::options::registerPy(m_options);

  register_tree_search(m);
}

} // namespace elf
