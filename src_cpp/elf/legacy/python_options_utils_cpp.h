/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include "elf/ai/tree_search/tree_search_options.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "pybind_helper.h"

struct ContextOptions {
  // How many simulation threads we are running.
  int num_games = 1;

  int batchsize = 0;

  // History length. How long we should keep the history.
  int T = 1;

  std::string job_id;

  elf::ai::tree_search::TSOptions mcts_options;

  std::shared_ptr<spdlog::logger> _logger;

  ContextOptions()
      : _logger(elf::logging::getLogger("elf::legacy::ContextOptions-", "")) {}

  void print() const {
    _logger->info("JobId: {}", job_id);
    _logger->info("#Game: {}", num_games);
    _logger->info("T: {}", T);
    _logger->info("{}", mcts_options.info());
  }

  REGISTER_PYBIND_FIELDS(job_id, batchsize, num_games, T, mcts_options);
};
