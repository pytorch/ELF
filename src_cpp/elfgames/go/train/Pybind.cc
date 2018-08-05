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

#include "distri_client.h"
#include "distri_server.h"
#include "elf/base/game_context.h"
#include "elf/options/reflection_option.h"
#include "elf/utils/pybind.h"
#include "elf/utils/reflection_utils.h"

namespace elfgames {
namespace go {

void getServerPredefined(elf::options::OptionSpec& spec) {
  elf::options::Visitor<GameOptionsTrain> visitor("", spec);
}

void getClientPredefined(elf::options::OptionSpec& spec) {
  elf::options::Visitor<GameOptionsSelfPlay> visitor("", spec);
}

GameOptionsTrain getServerOpt(const elf::options::OptionSpec& spec) {
  GameOptionsTrain options;
  elf::options::Saver<GameOptionsTrain> visitor("", spec, options);
  return options;
}

GameOptionsSelfPlay getClientOpt(
    const elf::options::OptionSpec& spec,
    std::string job_id) {
  GameOptionsSelfPlay options;
  elf::options::Saver<GameOptionsSelfPlay> visitor("", spec, options);
  options.common.base.job_id = job_id;
  return options;
}

std::string print_info_server(const GameOptionsTrain& options) {
  elf_utils::reflection::Printer printer;
  return printer.info<GameOptionsTrain>(options);
}

std::string print_info_client(const GameOptionsSelfPlay& options) {
  elf_utils::reflection::Printer printer;
  return printer.info<GameOptionsSelfPlay>(options);
}

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  auto ref = py::return_value_policy::reference_internal;

  PB_INIT(GameOptions)
  PB_FIELD(base)
  PB_FIELD(net)
  PB_FIELD(mcts)
  PB_END

  PB_INIT(GameOptionsTrain)
  PB_FIELD(common)
      .def("info", &print_info_server) PB_END

          PB_INIT(GameOptionsSelfPlay) PB_FIELD(common)
      .def("info", &print_info_client) PB_END

          m.def("getServerPredefined", getServerPredefined);
  m.def("getClientPredefined", getClientPredefined);

  m.def("getServerOpt", getServerOpt);
  m.def("getClientOpt", getClientOpt);

  py::class_<Server>(m, "Server")
      .def(py::init<const GameOptionsTrain&>())
      .def("setGameContext", &Server::setGameContext)
      .def("getParams", &Server::getParams)
      .def("waitForSufficientSelfplay", &Server::waitForSufficientSelfplay)
      .def("notifyNewVersion", &Server::notifyNewVersion)
      .def("setInitialVersion", &Server::setInitialVersion)
      .def("setEvalMode", &Server::setEvalMode);

  py::class_<Client>(m, "Client")
      .def(py::init<const GameOptionsSelfPlay&>())
      .def("setGameContext", &Client::setGameContext)
      .def("getGame", &Client::getGame, ref)
      .def("getParams", &Client::getParams)
      .def("setRequest", &Client::setRequest)
      .def("getGameStats", &Client::getGameStats, ref);

  py::class_<WinRateStats>(m, "WinRateStats")
      .def(py::init<>())
      .def("getBlackWins", &WinRateStats::getBlackWins)
      .def("getWhiteWins", &WinRateStats::getWhiteWins)
      .def("getTotalGames", &WinRateStats::getTotalGames);

  py::class_<GameStats>(m, "GameStats")
      .def("getWinRateStats", &GameStats::getWinRateStats)
      //.def("AllGamesFinished", &GameStats::AllGamesFinished)
      //.def("restartAllGames", &GameStats::restartAllGames)
      .def("getPlayedGames", &GameStats::getPlayedGames);

  py::class_<GoGameSelfPlay>(m, "GoGameSelfPlay")
      .def("showBoard", &GoGameSelfPlay::showBoard)
      .def("peekMCTS", &GoGameSelfPlay::peekMCTS)
      .def("addMCTSParams", &GoGameSelfPlay::addMCTSParams)
      .def("getNextPlayer", &GoGameSelfPlay::getNextPlayer)
      .def("getLastMove", &GoGameSelfPlay::getLastMove)
      .def("getScore", &GoGameSelfPlay::getScore)
      .def("getLastScore", &GoGameSelfPlay::getLastScore);
}

} // namespace go
} // namespace elfgames
