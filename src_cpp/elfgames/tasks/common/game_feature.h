/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */


#pragma once

#define OUTSIZE 2

#include "../base/go_state.h"
#include "go_game_specific.h"
#include "go_state_ext.h"

#include "elf/base/extractor.h"

#define UNUSED(expr) do { (void)(expr); } while (0)


enum SpecialActionType { SA_SKIP = -100, SA_PASS, SA_RESIGN, SA_CLEAR };

class ChouFleurFeature {
 public:
  ChouFleurFeature(const GameOptions& options) : options_(options) {
   //std::cout << "ChouFleurFeature" << std::endl;
   if (options.use_df_feature) {
      /*_num_plane = MAX_NUM_FEATURE;
      _our_stone_plane = OUR_STONES;
      _opponent_stone_plane = OPPONENT_STONES;*/
    } else {
      /*_num_plane = MAX_NUM_AGZ_FEATURE;
      _our_stone_plane = 0;
      _opponent_stone_plane = 1;*/
    }
    _num_plane = 1;  // FIXME
  }

  // Inference part.
  static void extractState(const BoardFeature& bf, float* f) {
    //std::cout << "extractState1 for ChouFleur" << std::endl;
    bf.extract(f);
    //std::cout << "extractState1ok for ChouFleur" << std::endl;
  }

  static void extractStateAGZ(const BoardFeature& bf, float* f) {
    //std::cout << "extractState2 for ChouFleur" << std::endl;
    bf.extractAGZ(f);
    //std::cout << "extractState2ok for ChouFleur" << std::endl;
  }

  static void ReplyValue(ChouFleurReply& reply, const float* value) {
    //std::cout << "ReplyValue for ChouFleur" << std::endl;
    reply.value = *value;
    //std::cout << "ReplyValueok for ChouFleur" << std::endl;
  }

  static void ReplyPolicy(ChouFleurReply& reply, const float* pi) {
    //std::cout << "ReplyPolicy for ChouFleur" << std::endl;
    copy(pi, pi + reply.pi.size(), reply.pi.begin());
    //std::cout << "ReplyPolicyok for ChouFleur" << std::endl;
  }

  static void ReplyAction(ChouFleurReply& reply, const int64_t* action) {
    std::cout << "ReplyAction" << std::endl;
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
        reply.c = reply.bf.action2Coord(*action);   // FIXME
    }
    std::cout << "ReplyActionok " << reply.c << "}}" << std::endl;
  }

  static void ReplyVersion(ChouFleurReply& reply, const int64_t* ver) {
    reply.version = *ver;
  }

  /////////////
  // Training part.
  static void extractMoveIdx(const ChouFleurStateExtOffline& s, int* move_idx) {
    *move_idx = s._state.getPly() - 1;
  }

  static void extractNumMove(const ChouFleurStateExtOffline& s, int* num_move) {
    *num_move = s.getNumMoves();
  }

  static void extractPredictedValue(
      const ChouFleurStateExtOffline& s,
      float* predicted_value) {
    *predicted_value = s.getPredictedValue(s._state.getPly() - 1);
  }

  static void extractAugCode(const ChouFleurStateExtOffline& s, int* code) {
    UNUSED(s);
    UNUSED(code);
    //*code = s._bf.getD4Code();
    std::cout << "no idea what is this bug" << std::endl;
    exit(-1);
  }

  static void extractWinner(const ChouFleurStateExtOffline& s, float* winner) {
    *winner = s._offline_winner;
  }

  static void extractStateExt(const ChouFleurStateExtOffline& s, float* f) {
    std::cout << "extractYOState for ChouFleur  ---  BUG" << std::endl;
    // Then send the data to the server.
    exit(-1);  // I believe FIXME that this is not used so I can just put exit(-1)
    extractState(s._bf, f);
  }

  static void extractStateExtAGZ(const ChouFleurStateExtOffline& s, float* f) {
    //std::cout << "extractStateExtAGZ for ChouFleur" << std::endl;
    // Then send the data to the server.
    extractStateAGZ(s._bf, f);
    //std::cout << "extractStateExtAGZOk for ChouFleur" << std::endl;
  }

  static void extractMCTSPi(const ChouFleurStateExtOffline& s, float* mcts_scores) {
    std::cout << "extractMCTSPi for ChouFleur" << std::endl;
    const BoardFeature& bf = s._bf;
    const size_t move_to = s._state.getPly() - 1;
    unsigned int BOARD_NUM_ACTION = OUTSIZE;  //StateForChouFleurNumActions;  TODO FIXME
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
        std::cout << "mcts_score" << i << " = " << mcts_scores[i] << std::endl;
      }
    } else {
//      mcts_scores[bf.coord2Action(s._offline_all_moves[move_to])] = 1.0;
      mcts_scores[bf.coord2Action(s._offline_all_moves[move_to])] = 1.0;
    }
    std::cout << "extractMCTSPiok for ChouFleur" << std::endl;
  }

  static void extractOfflineAction(
      const ChouFleurStateExtOffline& s,
      int64_t* offline_a) {
    //std::cout << "extractOfflineAction/gf" << std::endl;
    const BoardFeature& bf = s._bf;

    std::fill(offline_a, offline_a + s._options.num_future_actions, 0);
    const size_t move_to = s._state.getPly() - 1;
    for (int i = 0; i < s._options.num_future_actions; ++i) {
      Coord m = s._offline_all_moves[move_to + i];
      offline_a[i] = bf.coord2Action(m);
    }
  }

  static void extractStateSelfplayVersion(
      const ChouFleurStateExtOffline& s,
      int64_t* ver) {
    //std::cout << "extractStateSelfplayVersion/gf" << std::endl;
    *ver = s.curr_request_.vers.black_ver;
  }

  static void extractAIModelBlackVersion(const ModelPair& msg, int64_t* ver) {
    //std::cout << "extractAIModelBlackVersion/gf" << std::endl;
    *ver = msg.black_ver;
  }

  static void extractAIModelWhiteVersion(const ModelPair& msg, int64_t* ver) {
    //std::cout << "extractAIModelWhiteVersion/gf" << std::endl;
    *ver = msg.white_ver;
  }

  static void extractSelfplayVersion(const MsgVersion& msg, int64_t* ver) {
    //std::cout << "extractSelfPlayVersion/gf" << std::endl;
    *ver = msg.model_ver;
  }

  void registerExtractor(int batchsize, elf::Extractor& e) {
    // Register multiple fields.
    std::cout << " registerExtractor" << std::endl;
    auto& s = e.addField<float>("s").addExtents(
        batchsize, {batchsize, StateForChouFleurX, StateForChouFleurY, StateForChouFleurZ});  // FIXME
    if (options_.use_df_feature) {
      s.addFunction<BoardFeature>(extractState)
          .addFunction<ChouFleurStateExtOffline>(extractStateExt);
    } else {
      s.addFunction<BoardFeature>(extractStateAGZ)
          .addFunction<ChouFleurStateExtOffline>(extractStateExtAGZ);
    }

    std::cout << "middle - registerExtractor" << std::endl;
    unsigned int BOARD_NUM_ACTION = OUTSIZE; //StateForChouFleurNumActions;   TODO FIXME
    e.addField<int64_t>("a").addExtent(batchsize);
    e.addField<int64_t>("rv").addExtent(batchsize);
    e.addField<int64_t>("offline_a")
        .addExtents(batchsize, {batchsize, options_.num_future_actions});
    e.addField<float>({"V", "winner", "predicted_value"}).addExtent(batchsize);
    e.addField<float>({"pi", "mcts_scores"})
        .addExtents(batchsize, {batchsize, static_cast<int>(BOARD_NUM_ACTION)});
    e.addField<int32_t>({"move_idx", "aug_code", "num_move"})
        .addExtent(batchsize);

    e.addField<int64_t>({"black_ver", "white_ver", "selfplay_ver"})
        .addExtent(batchsize);

    e.addClass<ChouFleurReply>()
        .addFunction<int64_t>("a", ReplyAction)
        .addFunction<float>("pi", ReplyPolicy)
        .addFunction<float>("V", ReplyValue)
        .addFunction<int64_t>("rv", ReplyVersion);

    e.addClass<ChouFleurStateExtOffline>()
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
    std::cout << " registerExtractor ok" << std::endl;
  }

  std::map<std::string, int> getParams() const {
    unsigned int BOARD_NUM_ACTION = StateForChouFleurNumActions;
    return std::map<std::string, int>{
        {"num_action", static_cast<int>(BOARD_NUM_ACTION)},
        {"board_size", -1},
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
