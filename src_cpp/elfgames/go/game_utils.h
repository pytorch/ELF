/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <map>
#include <random>
#include "elf/legacy/pybind_helper.h"

struct ResignCheck {
  float resign_thres = 0.05;
  float never_resign_ratio = 0.1;

  bool never_resign = false;
  bool has_calculated_never_resign = false;

  ResignCheck(float thres, float never_resign_ratio)
      : resign_thres(thres), never_resign_ratio(never_resign_ratio) {}

  bool check(float value, std::mt19937* rng) {
    if (!has_calculated_never_resign) {
      std::uniform_real_distribution<> dis(0.0, 1.0);
      never_resign = (dis(*rng) < never_resign_ratio);
      has_calculated_never_resign = true;
    }

    if (never_resign) {
      return false;
    }

    if (value >= -1.0 + resign_thres)
      return false;

    return true;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[ResThres=" << resign_thres
       << "][NeverResignRatio=" << never_resign_ratio
       << "][NeverRes=" << never_resign << "]";
    return ss.str();
  }

  void reset() {
    never_resign = false;
    has_calculated_never_resign = false;
  }
};

struct Ranking {
  std::vector<uint64_t> counts;
  uint64_t total_count;

  Ranking(int max_rank = 10) : counts(max_rank + 1), total_count(0) {}

  void feed(int r) {
    if (r < (int)counts.size())
      counts[r]++;
    total_count++;
  }

  void reset() {
    std::fill(counts.begin(), counts.end(), 0);
    total_count = 0;
  }

  std::string info() const {
    std::stringstream ss;

    ss << "Total count: " << total_count << std::endl;
    for (size_t i = 0; i < counts.size(); ++i) {
      ss << "[" << i << "]: " << counts[i] << " ("
         << 100.0 * counts[i] / total_count << "%)" << std::endl;
    }

    return ss.str();
  }
};

struct WinRateStats {
  uint64_t black_wins = 0, white_wins = 0;
  float sum_reward = 0.0;
  uint64_t total_games = 0;

  void feed(float reward) {
    if (reward > 0)
      black_wins++;
    else
      white_wins++;
    sum_reward += reward;
    total_games++;
  }

  void reset() {
    black_wins = white_wins = 0;
    sum_reward = 0.0;
    total_games = 0;
  }

  REGISTER_PYBIND_FIELDS(black_wins, white_wins, sum_reward, total_games);
};
