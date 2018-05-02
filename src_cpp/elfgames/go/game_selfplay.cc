/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "game_selfplay.h"
#include "go_game_specific.h"
#include "mcts/ai.h"
#include "mcts/mcts.h"

#include <fstream>

////////////////// GoGame /////////////////////
GoGameSelfPlay::GoGameSelfPlay(
    int game_idx,
    elf::GameClient* client,
    const ContextOptions& context_options,
    const GameOptions& options,
    EvalCtrl* eval_ctrl)
    : GoGameBase(game_idx, client, context_options, options),
      eval_ctrl_(eval_ctrl),
      _state_ext(game_idx, options) {}

MCTSGoAI* GoGameSelfPlay::init_ai(
    const std::string& actor_name,
    const elf::ai::tree_search::TSOptions& mcts_options,
    float puct_override,
    int mcts_rollout_per_batch_override,
    int mcts_rollout_per_thread_override,
    int64_t model_ver) {
  /*
  cout << "Initialize " << actor_name << ", puct_override: " << puct_override
    << ", batch_override: " << mcts_rollout_per_batch_override
    << ", thread_override: " << mcts_rollout_per_thread_override << endl;
  */

  MCTSActorParams params;
  params.actor_name = actor_name;
  params.seed = _rng();
  params.ply_pass_enabled = _options.ply_pass_enabled;
  params.komi = _options.komi;
  params.required_version = model_ver;

  elf::ai::tree_search::TSOptions opt = mcts_options;
  if (puct_override > 0.0) {
    std::cout << "PUCT overridden: " << opt.alg_opt.c_puct << " -> "
              << puct_override << std::endl;
    opt.alg_opt.c_puct = puct_override;
  }
  if (mcts_rollout_per_batch_override > 0) {
    std::cout << "mcts_rollout_per_batch_override overridden: "
              << opt.num_rollouts_per_batch << " -> "
              << mcts_rollout_per_batch_override << std::endl;
    opt.num_rollouts_per_batch = mcts_rollout_per_batch_override;
  }
  if (mcts_rollout_per_thread_override > 0) {
    std::cout << "mcts_rollout_per_thread_override overridden: "
              << opt.num_rollouts_per_thread << " -> "
              << mcts_rollout_per_thread_override << std::endl;
    opt.num_rollouts_per_thread = mcts_rollout_per_thread_override;
  }

  return new MCTSGoAI(opt, [&](int) { return new MCTSActor(client_, params); });
}

Coord GoGameSelfPlay::mcts_make_diverse_move(MCTSGoAI* mcts_go_ai, Coord c) {
  auto policy = mcts_go_ai->getMCTSPolicy();

  bool diverse_policy =
      _state_ext.state().getPly() <= _options.policy_distri_cutoff;
  if (diverse_policy) {
    // Sample from the policy.
    c = policy.sampleAction(&_rng);
    /*
    if (show_board) {
        cout << "Move changed to [" << c << "][" << coord2str(c) << "][" <<
    coord2str2(c) << "]" << endl;
    }
    */
  }
  if (_options.policy_distri_training_for_all || diverse_policy) {
    // [TODO]: Warning: MCTS Policy might not correspond to move idx.
    _state_ext.addMCTSPolicy(policy);
  }

  return c;
}

Coord GoGameSelfPlay::mcts_update_info(MCTSGoAI* mcts_go_ai, Coord c) {
  float predicted_value = mcts_go_ai->getValue();

  _state_ext.addPredictedValue(predicted_value);

  if (!_options.dump_record_prefix.empty()) {
    _state_ext.saveCurrentTree(mcts_go_ai->getCurrentTree());
  }

  bool we_are_good = _state_ext.state().nextPlayer() == S_BLACK
      ? ((getScore() > 0) && (predicted_value > 0.9))
      : ((getScore() < 0) && (predicted_value < -0.9));
  // If the opponent wants pass, and we are in good, we follow.
  if (_human_player != nullptr && we_are_good &&
      _state_ext.state().lastMove() == M_PASS && _options.following_pass)
    c = M_PASS;

  // Check the ranking of selected move.
  auto move_rank = mcts_go_ai->getLastResult().getRank(
      c, elf::ai::tree_search::MCTSResultT<Coord>::PRIOR);
  eval_ctrl_->getGameStats().feedMoveRanking(move_rank.first);

  return c;
}

void GoGameSelfPlay::finish_game(FinishReason reason) {
  if (!_state_ext.currRequest().vers.is_selfplay() &&
      _options.cheat_eval_new_model_wins_half) {
    reason = FR_CHEAT_NEWER_WINS_HALF;
  }
  if (_state_ext.currRequest().vers.is_selfplay() &&
      _options.cheat_selfplay_random_result) {
    reason = FR_CHEAT_SELFPLAY_RANDOM_RESULT;
  }

  _state_ext.setFinalValue(reason, &_rng);
  _state_ext.showFinishInfo(reason);

  if (!_options.dump_record_prefix.empty()) {
    _state_ext.dumpSgf();
  }

  if (_options.print_result) {
    // lock_guard<mutex> lock(_mutex);
    // cout << endl << (final_value > 0 ? "Black" : "White") << " win. Ply: " <<
    // _state.getPly() << ", Value: " << final_value << ", Predicted: " <<
    // predicted_value << endl;
  }

  // reset tree if MCTS_AI, otherwise just do nothing
  _ai->endGame(_state_ext.state());
  if (_ai2 != nullptr) {
    _ai2->endGame(_state_ext.state());
  }

  // tell python / remote
  eval_ctrl_->ctrl()->process(_state_ext);
  // clear state, MCTS polices et.al.
  _state_ext.restart();
}

void GoGameSelfPlay::check_new_request() {
  auto on_recv = [this](MsgRequest&& request) -> MsgRestart {
    bool is_waiting = request.vers.wait();
    bool is_prev_waiting = _state_ext.currRequest().vers.wait();

    if (_options.verbose && !(is_waiting && is_prev_waiting)) {
      std::cout << "Receive request: "
                << (!is_waiting ? request.info() : "[wait]");
      std::cout << ", old: "
                << (!is_prev_waiting ? _state_ext.currRequest().info()
                                     : "[wait]");
      std::cout << std::endl;
    }

    bool same_vers = (request.vers == _state_ext.currRequest().vers);
    bool same_player_swap =
        (request.client_ctrl.player_swap ==
         _state_ext.currRequest().client_ctrl.player_swap);

    bool async = request.client_ctrl.async;

    bool no_restart =
        (same_vers || async) && same_player_swap && !is_prev_waiting;

    // Then we need to reset everything.
    _state_ext.setRequest(request);

    if (is_waiting) {
      return MsgRestart(RestartReply::ONLY_WAIT, _game_idx);
    } else {
      if (!no_restart) {
        restart();
        return MsgRestart(RestartReply::UPDATE_MODEL, _game_idx);
      } else {
        if (!async)
          return MsgRestart(RestartReply::UPDATE_REQUEST_ONLY, _game_idx);
        else {
          setAsync();
          if (same_vers)
            return MsgRestart(RestartReply::UPDATE_REQUEST_ONLY, _game_idx);
          else
            return MsgRestart(RestartReply::UPDATE_MODEL_ASYNC, _game_idx);
        }
      }
    }
  };

  MsgRestart msg;
  do {
    msg = eval_ctrl_->BroadcastReceiveIfDifferent(
        _state_ext.currRequest(), on_recv);
  } while (msg.result == RestartReply::ONLY_WAIT);

  // Update current state.
  eval_ctrl_->updateState(_state_ext.getThreadState());
}

void GoGameSelfPlay::setAsync() {
  _ai->setRequiredVersion(-1);
  if (_ai2 != nullptr)
    _ai2->setRequiredVersion(-1);

  _state_ext.addCurrentModel();
}

void GoGameSelfPlay::restart() {
  const MsgRequest& request = _state_ext.currRequest();
  bool async = request.client_ctrl.async;

  _ai.reset(nullptr);
  _ai2.reset(nullptr);
  if (_options.mode == "selfplay") {
    _ai.reset(init_ai(
        "actor_black",
        request.vers.mcts_opt,
        -1.0,
        -1,
        -1,
        async ? -1 : request.vers.black_ver));
    if (request.vers.white_ver >= 0) {
      _ai2.reset(init_ai(
          "actor_white",
          request.vers.mcts_opt,
          _state_ext.options().white_puct,
          _state_ext.options().white_mcts_rollout_per_batch,
          _state_ext.options().white_mcts_rollout_per_thread,
          async ? -1 : request.vers.white_ver));
    }
    if (!request.vers.is_selfplay() && request.client_ctrl.player_swap) {
      // Swap the two pointer.
      swap(_ai, _ai2);
    }
  } else if (_options.mode == "online") {
    _ai.reset(init_ai(
        "actor_black",
        request.vers.mcts_opt,
        -1.0,
        -1,
        -1,
        request.vers.black_ver));
    _human_player.reset(new AI(client_, {"human_actor"}));
  } else {
    std::cout << "Unknown mode! " << _options.mode << std::endl;
    throw std::range_error("Unknown mode");
  }

  _state_ext.restart();

  if (!_options.preload_sgf.empty()) {
    // Load an SGF file and follow this sgf while playing.
    _preload_sgf.load(_options.preload_sgf);
    _sgf_iter = _preload_sgf.begin();
    int i = 0;
    while (!_sgf_iter.done() && i < _options.preload_sgf_move_to) {
      auto curr = _sgf_iter.getCurrMove();
      if (!_state_ext.forward(curr.move)) {
        std::cout << _state_ext.state().showBoard() << std::endl;
        std::cout << "Proposed move: "
                  << elf::ai::tree_search::ActionTrait<Coord>::to_string(
                         curr.move)
                  << std::endl;
        throw std::runtime_error("Preload sgf: move not valid!");
      }
      i++;
      ++_sgf_iter;
    }
  }
}

void GoGameSelfPlay::act() {
  if (_online_counter % 5 == 0) {
    // Check request every 5 times.
    check_new_request();
  }
  _online_counter++;

  bool show_board = (_options.verbose && _context_options.num_games == 1);
  const GoState& s = _state_ext.state();

  if (_human_player != nullptr) {
    do {
      if (s.terminated()) {
        finish_game(FR_ILLEGAL);
        return;
      }

      BoardFeature bf(s);
      GoReply reply(bf);
      _human_player->act(bf, &reply);
      // skip the current move, and ask the ai to move.
      if (reply.c == M_SKIP)
        break;
      if (reply.c == M_CLEAR) {
        if (!_state_ext.state().justStarted()) {
          finish_game(FR_CLEAR);
        }
        return;
      }

      if (reply.c == M_RESIGN) {
        finish_game(FR_RESIGN);
        return;
      }
      // Otherwise we forward.
      if (_state_ext.forward(reply.c)) {
        if (_state_ext.state().isTwoPass()) {
          // If the human opponent pass, we pass as well.
          finish_game(FR_TWO_PASSES);
        }
        return;
      }
      // cout << "Invalid move: x = " << X(c) << " y = " << Y(c) << " move: " <<
      // coord2str(c) << " please try again" << endl;
    } while (!client_->checkPrepareToStop());
  } else {
    // If re receive this, then we should not send games anymore
    // (otherwise the process never stops)
    if (client_->checkPrepareToStop()) {
      // [TODO] A lot of hack here. We need to fix it later.
      AI ai(client_, {"actor_black"});
      BoardFeature bf(s);
      GoReply reply(bf);
      ai.act(bf, &reply);

      if (client_->DoStopGames())
        return;

      AI ai_white(client_, {"actor_white"});
      ai_white.act(bf, &reply);

      elf::FuncsWithState funcs = client_->BindStateToFunctions(
          {"game_start"}, &_state_ext.currRequest().vers);
      client_->sendWait({"game_start"}, &funcs);

      funcs = client_->BindStateToFunctions({"game_end"}, &_state_ext.state());
      client_->sendWait({"game_end"}, &funcs);

      // std::cout << "Got prepare to stop .. " << endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      return;
    }
  }

  Stone player = s.nextPlayer();
  bool use_policy_network_only =
      (player == S_WHITE && _options.white_use_policy_network_only) ||
      (player == S_BLACK && _options.black_use_policy_network_only);

  Coord c = M_INVALID;
  MCTSGoAI* curr_ai =
      ((_ai2 != nullptr && player == S_WHITE) ? _ai2.get() : _ai.get());

  if (use_policy_network_only) {
    // Then we only use policy network to move.
    curr_ai->actPolicyOnly(s, &c);
  } else {
    curr_ai->act(s, &c);
    c = mcts_make_diverse_move(curr_ai, c);
  }
  c = mcts_update_info(curr_ai, c);

  if (show_board) {
    std::cout << "Current board: " << std::endl;
    std::cout << s.showBoard() << std::endl;
    std::cout << "[" << s.getPly() << "] Propose move "
              << elf::ai::tree_search::ActionTrait<Coord>::to_string(c)
              << std::endl;
  }

  const bool shouldResign = _state_ext.shouldResign(&_rng);
  if (shouldResign && s.getPly() >= 50) {
    finish_game(FR_RESIGN);
    return;
  }

  if (_preload_sgf.numMoves() > 0) {
    if (_sgf_iter.done()) {
      finish_game(FR_MAX_STEP);
      return;
    }
    Coord new_c = _sgf_iter.getCurrMove().move;
    std::cout << "[" << s.getPly() << "] Move changes from "
              << elf::ai::tree_search::ActionTrait<Coord>::to_string(c)
              << " to "
              << elf::ai::tree_search::ActionTrait<Coord>::to_string(new_c)
              << std::endl;
    c = new_c;
    ++_sgf_iter;
  }

  if (!_state_ext.forward(c)) {
    std::cout << "Something wrong! Move: " << c << "cannot be applied"
              << std::endl;
    std::cout << "Current board: " << std::endl;
    std::cout << s.showBoard() << std::endl;
    std::cout << "[" << s.getPly() << "] Propose move "
              << elf::ai::tree_search::ActionTrait<Coord>::to_string(c)
              << std::endl;
    std::cout << _state_ext.dumpSgf("") << std::endl;
    return;
  }

  if (s.terminated()) {
    finish_game(
        s.isTwoPass()
            ? FR_TWO_PASSES
            : s.getPly() >= BOARD_MAX_MOVE ? FR_MAX_STEP : FR_ILLEGAL);
  }

  if (_options.move_cutoff > 0 && s.getPly() >= _options.move_cutoff) {
    finish_game(FR_MAX_STEP);
  }
}
