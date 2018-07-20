/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "game_utils.h"

class GameStats {
 public:
  void feedMoveRanking(int ranking) {
    std::lock_guard<std::mutex> lock(_mutex);
    _move_ranking.feed(ranking);
  }

  void resetRankingIfNeeded(int num_reset_ranking) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_move_ranking.total_count > (uint64_t)num_reset_ranking) {
      std::cout << std::endl << _move_ranking.info() << std::endl;
      _move_ranking.reset();
    }
  }

  void feedWinRate(float final_value) {
    std::lock_guard<std::mutex> lock(_mutex);
    _win_rate_stats.feed(final_value);
  }

  void feedSgf(const std::string& sgf) {
    std::lock_guard<std::mutex> lock(_mutex);
    _sgfs.push_back(sgf);
  }

  // For sender.
  WinRateStats getWinRateStats() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _win_rate_stats;
  }

  std::vector<std::string> getPlayedGames() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _sgfs;
  }

 private:
  std::mutex _mutex;
  Ranking _move_ranking;
  WinRateStats _win_rate_stats;
  std::vector<std::string> _sgfs;
};
