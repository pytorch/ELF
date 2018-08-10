/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "go_common.h"

#include <random>
#include <vector>

#include "elf/logging/IndexedLoggerFactory.h"

#define MAX_NUM_FEATURE 25

#define OUR_LIB 0
#define OPPONENT_LIB 3
#define OUR_SIMPLE_KO 6
#define OUR_STONES 7
#define OPPONENT_STONES 8
#define EMPTY_STONES 9

// [TODO]: Other todo features.
#define OUR_HISTORY 10
#define OPPONENT_HISTORY 11
#define BORDER 12
#define POSITION_MARK 13
#define OUR_CLOSEST_COLOR 14
#define OPPONENT_CLOSEST_COLOR 15

#define BLACK_INDICATOR 16
#define WHITE_INDICATOR 17

#define MAX_NUM_AGZ_FEATURE 18
#define MAX_NUM_AGZ_HISTORY 8

struct BoardHistory {
  std::vector<Coord> black;
  std::vector<Coord> white;

  BoardHistory(const Board& b) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
      for (int j = 0; j < BOARD_SIZE; ++j) {
        Coord c = OFFSETXY(i, j);
        Stone s = b._infos[c].color;
        if (s == S_WHITE)
          white.push_back(c);
        else if (s == S_BLACK)
          black.push_back(c);
      }
    }
  }
};

class GoState;

class BoardFeature {
 public:
  enum Rot { NONE = 0, CCW90, CCW180, CCW270 };

  BoardFeature(const GoState& s, Rot rot, bool flip)
      : s_(s),
        _rot(rot),
        _flip(flip),
        logger_(
            elf::logging::getLogger("elfgames::go::base::BoardFeature-", "")) {}
  BoardFeature(const GoState& s) : s_(s), _rot(NONE), _flip(false) {}

  static BoardFeature RandomShuffle(const GoState& s, std::mt19937* rng) {
    BoardFeature bf(s);
    bf.setD4Code((*rng)() % 8);
    return bf;
  }

  const GoState& state() const {
    return s_;
  }

  void setD4Group(Rot new_rot, bool new_flip) {
    _rot = new_rot;
    _flip = new_flip;
  }
  void setD4Code(int code) {
    auto rot = (BoardFeature::Rot)(code % 4);
    bool flip = (code >> 2) == 1;
    setD4Group(rot, flip);
  }
  int getD4Code() const {
    return (int)_rot + ((_flip ? 1 : 0) << 2);
  }

  std::pair<int, int> Transform(const std::pair<int, int>& p) const {
    std::pair<int, int> output;

    if (_rot == CCW90)
      output = std::make_pair(p.second, BOARD_SIZE - p.first - 1);
    else if (_rot == CCW180)
      output =
          std::make_pair(BOARD_SIZE - p.first - 1, BOARD_SIZE - p.second - 1);
    else if (_rot == CCW270)
      output = std::make_pair(BOARD_SIZE - p.second - 1, p.first);
    else
      output = p;

    if (_flip)
      std::swap(output.first, output.second);
    return output;
  }

  std::pair<int, int> InvTransform(const std::pair<int, int>& p) const {
    std::pair<int, int> output(p);

    if (_flip)
      std::swap(output.first, output.second);

    if (_rot == CCW90)
      output = std::make_pair(BOARD_SIZE - output.second - 1, output.first);
    else if (_rot == CCW180)
      output = std::make_pair(
          BOARD_SIZE - output.first - 1, BOARD_SIZE - output.second - 1);
    else if (_rot == CCW270)
      output = std::make_pair(output.second, BOARD_SIZE - output.first - 1);

    return output;
  }

  int64_t coord2Action(Coord m) const {
    if (m == M_PASS)
      return BOARD_ACTION_PASS;
    auto p = Transform(std::make_pair(X(m), Y(m)));
    return EXPORT_OFFSET_XY(p.first, p.second);
  }

  Coord action2Coord(int64_t action) const {
    if (action == -1 || action == BOARD_ACTION_PASS)
      return M_PASS;
    auto p = InvTransform(std::make_pair(EXPORT_X(action), EXPORT_Y(action)));
    return OFFSETXY(p.first, p.second);
  }

  void extract(std::vector<float>* features) const;
  void extractAGZ(std::vector<float>* features) const;
  void extract(float* features) const;
  void extractAGZ(float* features) const;

 private:
  const GoState& s_;
  Rot _rot = NONE;
  bool _flip = false;

  static constexpr int64_t kBoardRegion = BOARD_SIZE * BOARD_SIZE;

  std::shared_ptr<spdlog::logger> logger_;

  int transform(int x, int y) const {
    auto p = Transform(std::make_pair(x, y));
    return EXPORT_OFFSET_XY(p.first, p.second);
  }

  int transform(Coord m) const {
    return transform(X(m), Y(m));
  }

  int transform(Coord m, int c) const {
    return transform(X(m), Y(m)) + c * kBoardRegion;
  }

  // Compute features.
  bool getLibertyMap3(Stone player, float* data) const;
  bool getLibertyMap(Stone player, float* data) const;
  bool getLibertyMap3binary(Stone player, float* data) const;
  bool getStones(Stone player, float* data) const;
  bool getSimpleKo(Stone player, float* data) const;
  bool getHistory(Stone player, float* data) const;
  bool getHistoryExp(Stone player, float* data) const;
  bool getDistanceMap(Stone player, float* data) const;
};
