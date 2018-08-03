/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "go_state_ext.h"

void replaceAll(
    std::string& s,
    const std::string& search,
    const std::string& replace) {
  for (size_t pos = 0;; pos += replace.length()) {
    // Locate the substring to replace
    pos = s.find(search, pos);
    if (pos == std::string::npos)
      break;
    // Replace by erasing and inserting
    s.erase(pos, search.length());
    s.insert(pos, replace);
  }
}

std::string GoStateExt::dumpSgf(const std::string& filename) const {
  std::vector<Coord> moves = _state.getAllMoves();

  std::stringstream ss;
  float value = _state.getFinalValue();
  std::string result;
  if (abs(value) == 1.0) {
    result = (value > 0.0 ? "B+R" : "W+R");
  } else {
    result =
        (value > 0.0 ? "B+" + std::to_string(value)
                     : "W+" + std::to_string(-value));
  }

  std::stringstream ss_overall_comment;
  ss_overall_comment << "Filename: " << filename << std::endl;
  ss_overall_comment << "Git hash: " << __STR(GIT_COMMIT_HASH) << std::endl;
  ss_overall_comment << "Staged: " << __STR(GIT_STAGED) << std::endl;

  ss << "(;SZ[" << BOARD_SIZE << "]RE[" << result
     << "]C[" + ss_overall_comment.str() + "]";
  std::string black_name = _options.use_mcts ? "MCTS" : "Policy";
  if (_options.black_use_policy_network_only)
    black_name += "(policy only)";

  bool white_mcts = _options.mode == "selfplay_eval" ? _options.use_mcts_ai2
                                                     : _options.use_mcts;

  std::string white_name = white_mcts ? "MCTS" : "Policy";
  if (_options.white_use_policy_network_only)
    white_name += "(policy only)";

  ss << "PB[" << black_name << "]PW[" << white_name << "]KM[" << _options.komi
     << "]";

  for (size_t i = 0; i < moves.size(); i++) {
    std::string color = (i % 2 == 0 ? "B" : "W");
    ss << ";" << color << "[" << coord2str(moves[i]) << "]";
    std::string comments;
    comments += std::to_string(i + 1) + ": ";
    if (i < _predicted_values.size()) {
      comments += "PredV: " + std::to_string(_predicted_values[i]); // + "\n";
    }
    /*
    if (i < _mcts_policies.size()) {
        string mcts_info = _mcts_policies[i].info_packed(coord2str);
        replaceAll(mcts_info, "[", "\\[");
        replaceAll(mcts_info, "]", "\\]");
        comments += mcts_info;
    }*/

    ss << "C[" << comments << "]";
  }
  ss << ")";
  return ss.str();
}

void GoStateExt::showFinishInfo(FinishReason reason) const {
  Stone player = _state.nextPlayer();
  _logger->info("{}\n", _state.showBoard());
  std::string sgf_record = dumpSgf("");
  _logger->info("{}\n", sgf_record);

  _logger->info("[{}:{}] Current request: {}, used_model: ",
    _game_idx,
    _seq,
    curr_request_.info());
  for (const auto& i : using_models_) {
    _logger->info("{}, ", i);
  }
  _logger->info("\n");

  switch (reason) {
    case FR_RESIGN:
      _logger->info("Player {} resigned at {} Resign Thres: {}",
        player2str(player),
        _state.getPly(),
        _resign_check.resign_thres);
      break;
    case FR_MAX_STEP:
      _logger->info("Ply: {} exceeds thread_state.Restarting the game",
        _state.getPly());
      break;
    case FR_TWO_PASSES:
      _logger->info("Both pass at {}", _state.getPly());
      break;
    case FR_ILLEGAL:
      _logger->info("Illegal move at {}", _state.getPly());
      break;
    case FR_CLEAR:
      _logger->info("Restarting at {}", _state.getPly());
      break;
    case FR_CHEAT_NEWER_WINS_HALF:
      _logger->info("Cheat mode: Version: {}, swap: {}",
        curr_request_.vers.info(),
        curr_request_.client_ctrl.player_swap);
      break;
    case FR_CHEAT_SELFPLAY_RANDOM_RESULT:
      _logger->info("Cheat selfplay mode: Version: {}, swap: {}",
        curr_request_.vers.info(),
        curr_request_.client_ctrl.player_swap);
      break;
  }
  _logger->info(", Value: {}, Predicted: {}, ResCheck: {}\n",
    _state.getFinalValue(),
    getLastPredictedValue(),
    _resign_check.info());
}
