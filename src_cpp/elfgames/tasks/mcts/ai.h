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

//#include "elfgames/tasks/Base.h"
#include "elfgames/tasks/base/go_state.h"

#define M_SKIP 2
#define M_INVALID 3
#define M_CLEAR 4

#define UNUSED(expr) do { (void)(expr); } while (0)

using AI = elf::ai::AIClientT<BoardFeature, ChouFleurReply>;

namespace elf {
namespace ai {
namespace tree_search {

template <>
struct ActionTrait<Coord> {
 public:
  static std::string to_string(const Coord& c) {
    UNUSED(c);
    /*return "[" + coord2str2(c) + "][" + coord2str(c) + "][" +
        std::to_string(c) + "]";*/
    return "FIXME";
  }
  static Coord default_value() {
    return M_INVALID;
  }
};

template <>
struct StateTrait<StateForChouFleur, Coord> {
 public:
  static std::string to_string(const StateForChouFleur& s) {
    return "tt score (no komi): " + std::to_string(s._GetStatus());
  }
  static bool equals(const StateForChouFleur& s1, const StateForChouFleur& s2) {
    return s1.GetHash() == s2.GetHash();
  }

  static bool moves_since(
      const StateForChouFleur& s,
      size_t* next_move_number,
      std::vector<Coord>* moves) {
    return s.moves_since(next_move_number, moves);
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
