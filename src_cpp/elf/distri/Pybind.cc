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

#include "client.h"
#include "server.h"
#include "elf/base/game_context.h"
#include "elf/options/reflection_option.h"
#include "elf/utils/pybind.h"
#include "elf/utils/reflection_utils.h"
#include "elf/options/pybind_utils.h"

namespace elf {

namespace cs {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  // auto ref = py::return_value_policy::reference_internal;
  options::PyInterface<Options>(m, "DistributedOptions");

  py::class_<Server>(m, "Server")
      .def(py::init<const Options&>())
      .def("setGameContext", &Server::setGameContext)
      .def("setFactory", &Server::setFactory)
      .def("getParams", &Server::getParams);

  py::class_<Client>(m, "Client")
      .def(py::init<const Options&>())
      .def("setGameContext", &Client::setGameContext)
      .def("setFactory", &Client::setFactory)
      .def("getParams", &Client::getParams);
}

}  // namespace cs

}  // namespace elf
