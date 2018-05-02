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

#include "elfgames/go/game_context.h"

namespace elfgames {
namespace go {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  auto ref = py::return_value_policy::reference_internal;

  py::class_<GameContext>(m, "GameContext")
      .def(py::init<const ContextOptions&, const GameOptions&>())
      .def("ctx", &GameContext::ctx, ref)
      .def("getParams", &GameContext::getParams)
      .def("getGame", &GameContext::getGame, ref)
      .def("waitForSufficientSelfplay", &GameContext::waitForSufficientSelfplay)
      .def("notifyNewVersion", &GameContext::notifyNewVersion)
      .def("setInitialVersion", &GameContext::setInitialVersion)
      .def("setRequest", &GameContext::setRequest)
      .def("setEvalMode", &GameContext::setEvalMode)
      .def("getGameStats", &GameContext::getGameStats, ref);

  // Also register other objects.
  PYCLASS_WITH_FIELDS(m, ContextOptions)
      .def(py::init<>())
      .def("print", &ContextOptions::print);

  PYCLASS_WITH_FIELDS(m, GameOptions)
      .def(py::init<>())
      .def("info", &GameOptions::info);

  PYCLASS_WITH_FIELDS(m, WinRateStats).def(py::init<>());

  py::class_<GameStats>(m, "GameStats")
      .def("getWinRateStats", &GameStats::getWinRateStats)
      //.def("AllGamesFinished", &GameStats::AllGamesFinished)
      //.def("restartAllGames", &GameStats::restartAllGames)
      .def("getPlayedGames", &GameStats::getPlayedGames);

  py::class_<GoGameSelfPlay>(m, "GoGameSelfPlay")
      .def("showBoard", &GoGameSelfPlay::showBoard)
      .def("getNextPlayer", &GoGameSelfPlay::getNextPlayer)
      .def("getLastMove", &GoGameSelfPlay::getLastMove)
      .def("getScore", &GoGameSelfPlay::getScore)
      .def("getLastScore", &GoGameSelfPlay::getLastScore);
}

} // namespace go
} // namespace elfgames
