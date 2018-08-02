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

#include "elf/base/game_context.h"
#include "game_context.h"

namespace elfgames {
namespace go {

elf::Options get_elf_options(const GameOptions& game_options) {
  return game_options.base;
}

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  auto ref = py::return_value_policy::reference_internal;

  m.def("get_elf_options", get_elf_options);

  py::class_<Client>(m, "Client")
      .def(py::init<const GameOptionsSelfPlay&>())
      .def("setGameContext", &Client::setGameContext)
      .def("getParams", &Client::getParams)
      .def("setRequest", &Client::setRequest)
      .def("getGame", &Client::getGameC, ref);

  py::class_<GoGameSelfPlay>(m, "GoGameSelfPlay")
      .def("showBoard", &GoGameSelfPlay::showBoard)
      .def("getNextPlayer", &GoGameSelfPlay::getNextPlayer)
      .def("getLastMove", &GoGameSelfPlay::getLastMove)
      .def("getScore", &GoGameSelfPlay::getScore)
      .def("getLastScore", &GoGameSelfPlay::getLastScore);
}

} // namespace go
} // namespace elfgames
