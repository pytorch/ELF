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
#include "pybind_helper.h"

struct ContextOptions {
  // How many simulation threads we are running.
  int num_games = 1;

  int batchsize = 0;

  // History length. How long we should keep the history.
  int T = 1;

  // verbose options.
  bool verbose_comm = false;

  std::string job_id;

  elf::ai::tree_search::TSOptions mcts_options;

  ContextOptions() {}

  void print() const {
    std::cout << "JobId: " << job_id << std::endl;
    std::cout << "#Game: " << num_games << std::endl;
    std::cout << "T: " << T << std::endl;
    if (verbose_comm)
      std::cout << "Comm Verbose On" << std::endl;
    std::cout << mcts_options.info() << std::endl;
  }

  REGISTER_PYBIND_FIELDS(
      job_id,
      batchsize,
      num_games,
      T,
      verbose_comm,
      mcts_options);
};
