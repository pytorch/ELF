/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "../base/go_state.h"
#include "go_game_specific.h"
#include "go_state_ext.h"

#include "elf/base/extractor.h"

enum SpecialActionType { SA_SKIP = -100, SA_PASS, SA_RESIGN, SA_CLEAR };

class GoFeature {
 public:
  GoFeature(const GameOptions& options) : options_(options) {
    if (options.use_df_feature) {
      _num_plane = MAX_NUM_FEATURE;
      _our_stone_plane = OUR_STONES;
      _opponent_stone_plane = OPPONENT_STONES;
    } else {
      _num_plane = MAX_NUM_AGZ_FEATURE;
      _our_stone_plane = 0;
      _opponent_stone_plane = 1;
    }
  }

  // Inference part.
  static void extractState(const BoardFeature& bf, float* f) {
    bf.extract(f);
  }

  static void extractStateAGZ(const BoardFeature& bf, float* f) {
    bf.extractAGZ(f);
  }

  static void ReplyValue(GoReply& reply, const float* value) {
    reply.value = *value;
  }

  static void ReplyPolicy(GoReply& reply, const float* pi) {
    copy(pi, pi + reply.pi.size(), reply.pi.begin());
  }

  static void ReplyAction(GoReply& reply, const int64_t* action) {
    switch ((SpecialActionType)*action) {
      case SA_RESIGN:
        reply.c = M_RESIGN;
        break;
      case SA_SKIP:
        reply.c = M_SKIP;
        break;
      case SA_PASS:
        reply.c = M_PASS;
        break;
      case SA_CLEAR:
        reply.c = M_CLEAR;
        break;
      default:
        reply.c = reply.bf.action2Coord(*action);
    }
  }

  static void ReplyVersion(GoReply& reply, const int64_t* ver) {
    reply.version = *ver;
  }

  /////////////
  // Training part.
  static void extractMoveIdx(const GoStateExtOffline& s, int* move_idx) {
    *move_idx = s._state.getPly() - 1;
  }

  static void extractNumMove(const GoStateExtOffline& s, int* num_move) {
    *num_move = s.getNumMoves();
  }

  static void extractPredictedValue(
      const GoStateExtOffline& s,
      float* predicted_value) {
    *predicted_value = s.getPredictedValue(s._state.getPly() - 1);
  }

  static void extractAugCode(const GoStateExtOffline& s, int* code) {
    *code = s._bf.getD4Code();
  }

  static void extractWinner(const GoStateExtOffline& s, float* winner) {
    *winner = s._offline_winner;
  }

  static void extractStateExt(const GoStateExtOffline& s, float* f) {
    // Then send the data to the server.
    extractState(s._bf, f);
  }

  static void extractStateExtAGZ(const GoStateExtOffline& s, float* f) {
    // Then send the data to the server.
    extractStateAGZ(s._bf, f);
  }

  static void extractMCTSPi(const GoStateExtOffline& s, float* mcts_scores) {
    const BoardFeature& bf = s._bf;
    const size_t move_to = s._state.getPly() - 1;

    std::fill(mcts_scores, mcts_scores + BOARD_NUM_ACTION, 0.0);
    if (move_to < s._mcts_policies.size()) {
      const auto& policy = s._mcts_policies[move_to].prob;
      float sum_v = 0.0;
      for (size_t i = 0; i < BOARD_NUM_ACTION; ++i) {
        mcts_scores[i] = policy[bf.action2Coord(i)];
        sum_v += mcts_scores[i];
      }
      // Then we normalize.
      for (size_t i = 0; i < BOARD_NUM_ACTION; ++i) {
        mcts_scores[i] /= sum_v;
      }
    } else {
      mcts_scores[bf.coord2Action(s._offline_all_moves[move_to])] = 1.0;
    }
  }

  static void extractOfflineAction(
      const GoStateExtOffline& s,
      int64_t* offline_a) {
    const BoardFeature& bf = s._bf;

    std::fill(offline_a, offline_a + s._options.num_future_actions, 0);
    const size_t move_to = s._state.getPly() - 1;
    for (int i = 0; i < s._options.num_future_actions; ++i) {
      Coord m = s._offline_all_moves[move_to + i];
      offline_a[i] = bf.coord2Action(m);
    }
  }

  static void extractStateSelfplayVersion(
      const GoStateExtOffline& s,
      int64_t* ver) {
    *ver = s.curr_request_.vers.black_ver;
  }

  static void extractAIModelBlackVersion(const ModelPair& msg, int64_t* ver) {
    *ver = msg.black_ver;
  }

  static void extractAIModelWhiteVersion(const ModelPair& msg, int64_t* ver) {
    *ver = msg.white_ver;
  }

  static void extractSelfplayVersion(const MsgVersion& msg, int64_t* ver) {
    *ver = msg.model_ver;
  }

  void registerExtractor(int batchsize, elf::Extractor& e) {
    // Register multiple fields.
    auto& s = e.addField<float>("s").addExtents(
        batchsize, {batchsize, _num_plane, BOARD_SIZE, BOARD_SIZE});
    if (options_.use_df_feature) {
      s.addFunction<BoardFeature>(extractState)
          .addFunction<GoStateExtOffline>(extractStateExt);
    } else {
      s.addFunction<BoardFeature>(extractStateAGZ)
          .addFunction<GoStateExtOffline>(extractStateExtAGZ);
    }

    e.addField<int64_t>("a").addExtent(batchsize);
    e.addField<int64_t>("rv").addExtent(batchsize);
    e.addField<int64_t>("offline_a")
        .addExtents(batchsize, {batchsize, options_.num_future_actions});
    e.addField<float>({"V", "winner", "predicted_value"}).addExtent(batchsize);
    e.addField<float>({"pi", "mcts_scores"})
        .addExtents(batchsize, {batchsize, BOARD_NUM_ACTION});
    e.addField<int32_t>({"move_idx", "aug_code", "num_move"})
        .addExtent(batchsize);

    e.addField<int64_t>({"black_ver", "white_ver", "selfplay_ver"})
        .addExtent(batchsize);

    e.addClass<GoReply>()
        .addFunction<int64_t>("a", ReplyAction)
        .addFunction<float>("pi", ReplyPolicy)
        .addFunction<float>("V", ReplyValue)
        .addFunction<int64_t>("rv", ReplyVersion);

    e.addClass<GoStateExtOffline>()
        .addFunction<int32_t>("move_idx", extractMoveIdx)
        .addFunction<int32_t>("num_move", extractNumMove)
        .addFunction<float>("predicted_value", extractPredictedValue)
        .addFunction<int32_t>("aug_code", extractAugCode)
        .addFunction<float>("winner", extractWinner)
        .addFunction<float>("mcts_scores", extractMCTSPi)
        .addFunction<int64_t>("offline_a", extractOfflineAction)
        .addFunction<int64_t>("selfplay_ver", extractStateSelfplayVersion);

    e.addClass<ModelPair>()
        .addFunction<int64_t>("black_ver", extractAIModelBlackVersion)
        .addFunction<int64_t>("white_ver", extractAIModelWhiteVersion);

    e.addClass<MsgVersion>().addFunction<int64_t>(
        "selfplay_ver", extractSelfplayVersion);
  }

  std::map<std::string, int> getParams() const {
    return std::map<std::string, int>{
        {"num_action", BOARD_NUM_ACTION},
        {"board_size", BOARD_SIZE},
        {"num_future_actions", options_.num_future_actions},
        {"num_planes", _num_plane},
        {"our_stone_plane", _our_stone_plane},
        {"opponent_stone_plane", _opponent_stone_plane},
        {"ACTION_SKIP", SA_SKIP},
        {"ACTION_PASS", SA_PASS},
        {"ACTION_RESIGN", SA_RESIGN},
        {"ACTION_CLEAR", SA_CLEAR},
    };
  }

 private:
  GameOptions options_;

  int _num_plane;
  int _our_stone_plane;
  int _opponent_stone_plane;
};
