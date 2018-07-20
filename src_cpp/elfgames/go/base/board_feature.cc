/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "board_feature.h"
#include <cassert>
#include <cmath>
#include <utility>
#include "go_state.h"

#define S_ISA(c1, c2) ((c2 == S_EMPTY) || (c1 == c2))
// For feature extraction.
// Distance transform
static void DistanceTransform(float* arr) {
#define IND(i, j) ((i)*BOARD_SIZE + (j))
  // First dimension.
  for (int j = 0; j < BOARD_SIZE; j++) {
    for (int i = 1; i < BOARD_SIZE; i++) {
      arr[IND(i, j)] = std::min(arr[IND(i, j)], arr[IND(i - 1, j)] + 1);
    }
    for (int i = BOARD_SIZE - 2; i >= 0; i--) {
      arr[IND(i, j)] = std::min(arr[IND(i, j)], arr[IND(i + 1, j)] + 1);
    }
  }
  // Second dimension
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 1; j < BOARD_SIZE; j++) {
      arr[IND(i, j)] = std::min(arr[IND(i, j)], arr[IND(i, j - 1)] + 1);
    }
    for (int j = BOARD_SIZE - 2; j >= 0; j--) {
      arr[IND(i, j)] = std::min(arr[IND(i, j)], arr[IND(i, j + 1)] + 1);
    }
  }
#undef IDX
}

// If we set player = 0 (S_EMPTY), then the liberties of both side will be
// returned.
bool BoardFeature::getLibertyMap(Stone player, float* data) const {
  // We assume the output liberties is a 19x19 tensor.
  /*
  if (THTensor_nDimension(liberties) != 2) return false;
  if (THTensor_size(liberties, 1) != BOARD_SIZE) return false;
  if (THTensor_size(liberties, 2) != BOARD_SIZE) return false;
  float *data = THTensor_data(liberties);

  int stride = THTensor_stride(liberties, 1);
  */
  const Board* _board = &s_.board();

  memset(data, 0, kBoardRegion * sizeof(float));
  for (int i = 1; i < _board->_num_groups; ++i) {
    if (S_ISA(_board->_groups[i].color, player)) {
      int liberty = _board->_groups[i].liberties;
      TRAVERSE(_board, i, c) {
        data[transform(c)] = liberty;
      }
      ENDTRAVERSE
    }
  }

  return true;
}

bool BoardFeature::getLibertyMap3(Stone player, float* data) const {
  // We assume the output liberties is a 3x19x19 tensor.
  // == 1, == 2, >= 3
  const Board* _board = &s_.board();

  memset(data, 0, 3 * kBoardRegion * sizeof(float));
  for (int i = 1; i < _board->_num_groups; ++i) {
    if (S_ISA(_board->_groups[i].color, player)) {
      int liberty = _board->_groups[i].liberties;
      TRAVERSE(_board, i, c) {
        if (liberty == 1)
          data[transform(c, 0)] = liberty;
        else if (liberty == 2)
          data[transform(c, 1)] = liberty;
        else
          data[transform(c, 2)] = liberty;
      }
      ENDTRAVERSE
    }
  }

  return true;
}

bool BoardFeature::getLibertyMap3binary(Stone player, float* data) const {
  // We assume the output liberties is a 3x19x19 tensor.
  // == 1, == 2, >= 3
  const Board* _board = &s_.board();

  memset(data, 0, 3 * kBoardRegion * sizeof(float));
  for (int i = 1; i < _board->_num_groups; ++i) {
    if (S_ISA(_board->_groups[i].color, player)) {
      int liberty = _board->_groups[i].liberties;
      TRAVERSE(_board, i, c) {
        if (liberty == 1)
          data[transform(c, 0)] = 1.0;
        else if (liberty == 2)
          data[transform(c, 1)] = 1.0;
        else
          data[transform(c, 2)] = 1.0;
      }
      ENDTRAVERSE
    }
  }

  return true;
}

bool BoardFeature::getStones(Stone player, float* data) const {
  const Board* _board = &s_.board();
  //
  memset(data, 0, kBoardRegion * sizeof(float));
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (_board->_infos[c].color == player)
        data[transform(i, j)] = 1;
    }
  }
  return true;
}

bool BoardFeature::getSimpleKo(Stone /*player*/, float* data) const {
  const Board* _board = &s_.board();

  memset(data, 0, kBoardRegion * sizeof(float));
  Coord m = getSimpleKoLocation(_board, NULL);
  if (m != M_PASS) {
    data[transform(m)] = 1;
    return true;
  }
  return false;
}

// If player == S_EMPTY, get history of both sides.
bool BoardFeature::getHistory(Stone player, float* data) const {
  const Board* _board = &s_.board();

  memset(data, 0, kBoardRegion * sizeof(float));
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (S_ISA(_board->_infos[c].color, player))
        data[transform(i, j)] = _board->_infos[c].last_placed;
    }
  }
  return true;
}

bool BoardFeature::getHistoryExp(Stone player, float* data) const {
  const Board* _board = &s_.board();

  memset(data, 0, kBoardRegion * sizeof(float));
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (S_ISA(_board->_infos[c].color, player)) {
        data[transform(i, j)] =
            exp((_board->_infos[c].last_placed - _board->_ply) / 10.0);
      }
    }
  }
  return true;
}

bool BoardFeature::getDistanceMap(Stone player, float* data) const {
  const Board* _board = &s_.board();

  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (_board->_infos[c].color == player)
        data[transform(i, j)] = 0;
      else
        data[transform(i, j)] = 10000;
    }
  }
  DistanceTransform(data);
  return true;
}

static float* board_plane(float* features, int idx) {
  return features + idx * BOARD_SIZE * BOARD_SIZE;
}

#define LAYER(idx) board_plane(features, idx)

/* darkforestGo/utils/goutils.lua
extended = {
    "our liberties", "opponent liberties", "our simpleko", "our stones",
"opponent stones", "empty stones", "our history", "opponent history",
    "border", 'position_mask', 'closest_color'
},
*/

void BoardFeature::extract(std::vector<float>* features) const {
  features->resize(MAX_NUM_FEATURE * kBoardRegion);
  extract(&(*features)[0]);
}

void BoardFeature::extract(float* features) const {
  std::fill(features, features + MAX_NUM_FEATURE * kBoardRegion, 0.0);

  const Board* _board = &s_.board();

  Stone player = _board->_next_player;

  // Save the current board state to game state.
  getLibertyMap3binary(player, LAYER(OUR_LIB));
  getLibertyMap3binary(OPPONENT(player), LAYER(OPPONENT_LIB));
  getSimpleKo(player, LAYER(OUR_SIMPLE_KO));

  getStones(player, LAYER(OUR_STONES));
  getStones(OPPONENT(player), LAYER(OPPONENT_STONES));
  getStones(S_EMPTY, LAYER(EMPTY_STONES));

  getHistoryExp(player, LAYER(OUR_HISTORY));
  getHistoryExp(OPPONENT(player), LAYER(OPPONENT_HISTORY));

  getDistanceMap(player, LAYER(OUR_CLOSEST_COLOR));
  getDistanceMap(OPPONENT(player), LAYER(OPPONENT_CLOSEST_COLOR));

  float* black_indicator = LAYER(BLACK_INDICATOR);
  float* white_indicator = LAYER(WHITE_INDICATOR);
  if (player == S_BLACK)
    std::fill(black_indicator, black_indicator + kBoardRegion, 1.0);
  else
    std::fill(white_indicator, white_indicator + kBoardRegion, 1.0);
}

void BoardFeature::extractAGZ(std::vector<float>* features) const {
  features->resize(MAX_NUM_AGZ_FEATURE * kBoardRegion);
  extractAGZ(&(*features)[0]);
}

// Extract feature for One position
// Of size 18 * N * N
// store in float* features
void BoardFeature::extractAGZ(float* features) const {
  std::fill(features, features + MAX_NUM_AGZ_FEATURE * kBoardRegion, 0.0);

  const Board* _board = &s_.board();
  // get history of type std::deque<BoardHistory>
  // and BoardHistory is a struct with members:
  // std::vector<Coord> black;
  // std::vector<Coord> white;
  const auto& history = s_.getHistory();

  Stone player = _board->_next_player;

  if (history.size() > MAX_NUM_AGZ_HISTORY) {
    std::cout << "#history.size() = " << history.size() << ", > "
              << MAX_NUM_AGZ_HISTORY << std::endl;
    assert(false);
  }

  // Save the current board state to game state.
  int i = 0;
  for (auto it = history.rbegin(); it != history.rend(); ++it) {
    const std::vector<Coord>* myself = &it->black;
    const std::vector<Coord>* opponent = &it->white;
    if (player == S_WHITE)
      std::swap(myself, opponent);

    float* plane_myself = LAYER(i);
    for (Coord c : *myself)
      plane_myself[transform(c)] = 1.0;

    float* plane_opponent = LAYER(i + 1);
    for (Coord c : *opponent)
      plane_opponent[transform(c)] = 1.0;

    i += 2;
  }

  float* black_indicator = LAYER(2 * MAX_NUM_AGZ_HISTORY);
  float* white_indicator = LAYER(2 * MAX_NUM_AGZ_HISTORY + 1);
  if (player == S_BLACK)
    std::fill(black_indicator, black_indicator + kBoardRegion, 1.0);
  else
    std::fill(white_indicator, white_indicator + kBoardRegion, 1.0);
}
