/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "elf/ai/ai.h"
#include "elf/ai/tree_search/tree_search_base.h"

#include "elfgames/go/base/go_state.h"

struct GoHumanInfo {};
struct GoHumanReply {
  Coord c = M_INVALID;
  int64_t msec_ts_recv_cmd = -1;

  std::string info() const {
    std::stringstream ss;
    ss << "c=" << coord2str2(c) << ", ts recv_cmd: " << msec_ts_recv_cmd;
    return ss.str();
  }
};

using AI = elf::ai::AIClientT<BoardFeature, GoReply>;
using HumanPlayer = elf::ai::AIClientT<GoHumanInfo, GoHumanReply>;

namespace elf {
namespace ai {
namespace tree_search {

template <>
struct ActionTrait<Coord> {
 public:
  static std::string to_string(const Coord& c) {
    return "[" + coord2str2(c) + "][" + coord2str(c) + "][" +
        std::to_string(c) + "]";
  }
  static Coord default_value() {
    return M_INVALID;
  }
};

template <>
struct StateTrait<GoState, Coord> {
 public:
  static std::string to_string(const GoState& s) {
    return "tt score (no komi): " + std::to_string(s.evaluate(0));
  }
  static bool equals(const GoState& s1, const GoState& s2) {
    return s1.getHashCode() == s2.getHashCode();
  }

  static bool moves_since(
      const GoState& s,
      const GoState& s_ref,
      std::vector<Coord>* moves) {
    return s.moves_since(s_ref, moves);
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
