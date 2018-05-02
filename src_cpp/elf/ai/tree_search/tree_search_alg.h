/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <random>
#include <type_traits>
#include <utility>

#include "tree_search_base.h"

namespace elf {
namespace ai {
namespace tree_search {

template <typename Map>
MCTSResultT<typename Map::key_type> MostVisited(const Map& vals) {
  using A = typename Map::key_type;
  using MCTSResult = MCTSResultT<A>;
  static_assert(
      is_same<typename Map::mapped_type, EdgeInfo>::value,
      "key type must be EdgeInfo");

  MCTSResult res;
  for (const std::pair<A, EdgeInfo>& action_pair : vals) {
    const EdgeInfo& info = action_pair.second;

    res.feed(info.num_visits, action_pair);
  }
  return res;
};

template <typename Map>
MCTSResultT<typename Map::key_type> StrongestPrior(const Map& vals) {
  using A = typename Map::key_type;
  using MCTSResult = MCTSResultT<A>;
  static_assert(
      is_same<typename Map::mapped_type, EdgeInfo>::value,
      "key type must be EdgeInfo");

  MCTSResult res;
  for (const std::pair<A, EdgeInfo>& action_pair : vals) {
    const EdgeInfo& info = action_pair.second;

    res.feed(info.prior_probability, action_pair);
  }
  return res;
};

template <typename Map>
MCTSResultT<typename Map::key_type> UniformRandom(const Map& vals) {
  using A = typename Map::key_type;
  using MCTSResult = MCTSResultT<A>;
  static_assert(
      is_same<typename Map::mapped_type, EdgeInfo>::value,
      "key type must be EdgeInfo");

  static std::mt19937 rng(time(NULL));
  static std::mutex mu;

  MCTSResult res;

  int idx = 0;
  {
    std::lock_guard<mutex> lock(mu);
    idx = rng() % vals.size();
  }
  auto it = vals.begin();
  while (--idx >= 0) {
    ++it;
  }

  res.feed(it->second.num_visits, *it);
  return res;
};

} // namespace tree_search
} // namespace ai
} // namespace elf
