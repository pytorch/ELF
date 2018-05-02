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

using AI = elf::ai::AIClientT<BoardFeature, GoReply>;

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
      size_t* next_move_number,
      std::vector<Coord>* moves) {
    return s.moves_since(next_move_number, moves);
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
