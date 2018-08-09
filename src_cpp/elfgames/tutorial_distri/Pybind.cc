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

namespace elfgames {
namespace tutorial {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  // auto ref = py::return_value_policy::reference_internal;

  PB_INIT(GameOptions)
    PB_FIELD(base)
    PB_FIELD(net)
  PB_END

  py::class_<Server>(m, "Server")
      .def(py::init<const GameOptions&>())
      .def("setGameContext", &Server::setGameContext);

  py::class_<Client>(m, "Client")
      .def(py::init<const GameOptions&>())
      .def("setGameContext", &Client::setGameContext);
}

} // namespace tutorial
} // namespace elfgames
