/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sstream>
#include <string>
#include "elf/legacy/pybind_helper.h"
#include "elf/utils/utils.h"

struct GameOptions {
  // Seed.
  unsigned int seed;
  int num_future_actions = 3;

  // For offline training,
  //    This means how many games to open simultaneously per thread?
  //    When sending current situations, randomly select one to break any
  //    correlations.
  // For selfplay setting.
  //    This means how many games are played sequentially in each thread.
  int num_games_per_thread = -1;

  // mode == "online": it will open the game in online mode.
  //    In this mode, the thread will not output the next k moves (since every
  //    game is new).
  //    Instead, it will get the action from the neural network to proceed.
  // mode == "offline": offline training
  // mode == "selfplay": self play.
  std::string mode;

  // Use mcts engine.
  bool use_mcts = false;
  bool use_mcts_ai2 = false;

  // Specify which side uses policy network only.
  bool black_use_policy_network_only = false;
  bool white_use_policy_network_only = false;

  // -1 is random, 0-7 mean specific data aug.
  int data_aug = -1;

  // Before we use the data from the first recorded replay in each thread,
  // sample number of moves to fast-forward.
  // This is to increase the diversity of the game.
  // This number is sampled uniformly from [0, #Num moves * ratio_pre_moves]
  float start_ratio_pre_moves = 0.5;

  // Similar as above, but note that it will be applied to any newly loaded game
  // (except for the first one).
  // This will introduce bias in the dataset.
  // Useful when you want that (or when you want to visualize the data).
  float ratio_pre_moves = 0.0;

  // Cutoff ply for each loaded game, and start a new one.
  int move_cutoff = -1;

  // Cutoff ply for mcts policy / best a
  int policy_distri_cutoff = 20;
  bool policy_distri_training_for_all = false;

  float resign_thres = 0.05;
  float resign_thres_lower_bound = 1e-9;
  float resign_thres_upper_bound = 0.50;
  float resign_prob_never = 0.1;
  float resign_target_fp_rate = 0.05;
  int resign_target_hist_size = 2500;

  int num_reset_ranking = 5000;

  std::string preload_sgf;
  int preload_sgf_move_to = -1;

  bool use_df_feature = false;

  int q_min_size = 10;
  int q_max_size = 1000;
  int num_reader = 50;

  float komi = 7.5;
  int ply_pass_enabled = 0;

  // Second puct used for ai2, if -1 then use the same puct.
  float white_puct = -1.0;
  int white_mcts_rollout_per_batch = -1;
  int white_mcts_rollout_per_thread = -1;

  int eval_num_games = 400;
  float eval_thres = 0.55;

  // Default it is 20 min. During intergration test we could make it shorter.
  int client_max_delay_sec = 1200;

  // Initial number of selfplay games for each model used for selfplay.
  int selfplay_init_num = 5000;
  // Additive number of selfplay after the new model is updated.
  int selfplay_update_num = 1000;

  // Whether we use async mode for selfplay.
  bool selfplay_async = false;

  // When playing with human (or other programs), if human pass, we also pass.
  bool following_pass = false;

  bool cheat_eval_new_model_wins_half = false;
  bool cheat_selfplay_random_result = false;

  bool keep_prev_selfplay = false;

  int eval_num_threads = 1;
  int expected_num_clients = -1;

  // A list file containing the files to load.
  std::vector<std::string> list_files;
  std::string server_addr;
  std::string server_id;
  int port;
  bool verbose = false;
  bool print_result = false;
  std::string dump_record_prefix;

  std::string time_signature;

  GameOptions() {
    time_signature = elf_utils::time_signature();
  }

  std::string info() const {
    std::stringstream ss;
    ss << "Seed: " << seed << std::endl;
    ss << "Time signature: " << time_signature << std::endl;
    ss << "Client max delay in sec: " << client_max_delay_sec << std::endl;
    ss << "#FutureActions: " << num_future_actions << std::endl;
    ss << "#GamePerThread: " << num_games_per_thread << std::endl;
    ss << "mode: " << mode << std::endl;
    ss << "Selfplay init min #games: " << selfplay_init_num
       << ", update #games: " << selfplay_update_num
       << ", async: " << elf_utils::print_bool(selfplay_async) << std::endl;
    ss << "UseMCTS: " << elf_utils::print_bool(use_mcts) << std::endl;
    ss << "Data Aug: " << data_aug << std::endl;
    ss << "Start_ratio_pre_moves: " << start_ratio_pre_moves << std::endl;
    ss << "ratio_pre_moves: " << ratio_pre_moves << std::endl;
    ss << "MoveCutOff: " << move_cutoff << std::endl;
    ss << "Use DF feature: " << elf_utils::print_bool(use_df_feature)
       << std::endl;
    ss << "PolicyDistriCutOff: " << policy_distri_cutoff << std::endl;

    if (expected_num_clients > 0) {
      ss << "Expected #client: " << expected_num_clients << std::endl;
    }

    if (!list_files.empty()) {
      ss << "ListFile[" << list_files.size() << "]: ";
      for (const std::string& f : list_files) {
        ss << f << ", ";
      }
      ss << std::endl;
    }

    ss << "Server_addr: " << server_addr << ", server_id: " << server_id
       << ", port: " << port << std::endl;
    ss << "#Reader: " << num_reader << ", Qmin_sz: " << q_min_size
       << ", Qmax_sz: " << q_max_size << std::endl;
    ss << "Verbose: " << elf_utils::print_bool(verbose) << std::endl;
    ss << "Policy distri training for all moves: "
       << elf_utils::print_bool(verbose) << std::endl;
    ss << "Min Ply from which pass is enabled: " << ply_pass_enabled
       << std::endl;
    if (print_result)
      ss << "PrintResult: " << elf_utils::print_bool(print_result) << std::endl;
    if (!dump_record_prefix.empty())
      ss << "dumpRecord: " << dump_record_prefix << std::endl;
    if (following_pass)
      ss << "Following pass is true" << std::endl;
    ss << "Reset move ranking after " << num_reset_ranking << " actions"
       << std::endl;

    if (white_puct > 0.0)
      ss << "White puct: " << white_puct << std::endl;

    if (white_puct > 0.0)
      ss << "White puct: " << white_puct << std::endl;

    ss << "Resign Threshold: " << resign_thres << ", ";
    if (resign_prob_never > 0.0)
      ss << "Dynamic Resign Threshold, resign_prob_never: " << resign_prob_never
         << ", target_fp_rate: " << resign_target_fp_rate
         << ", bounded within [" << resign_thres_lower_bound << ", "
         << resign_thres_upper_bound << "]" << std::endl;

    if (black_use_policy_network_only)
      ss << "Black uses policy network only" << std::endl;
    if (white_use_policy_network_only)
      ss << "White uses policy network only" << std::endl;

    if (cheat_eval_new_model_wins_half)
      ss << "Cheat mode: New model gets 100% win rate half of the time."
         << std::endl;

    if (cheat_selfplay_random_result)
      ss << "Cheat selfplay mode: Random outcome." << std::endl;

    ss << std::endl;

    ss << "Komi: " << komi << std::endl;
    if (!preload_sgf.empty()) {
      ss << "Preload SGF:" << preload_sgf << ", move_to" << preload_sgf_move_to
         << std::endl;
    }
    return ss.str();
  }

  REGISTER_PYBIND_FIELDS(
      seed,
      mode,
      data_aug,
      start_ratio_pre_moves,
      ratio_pre_moves,
      move_cutoff,
      num_future_actions,
      list_files,
      verbose,
      num_games_per_thread,
      use_mcts,
      server_addr,
      server_id,
      port,
      policy_distri_cutoff,
      client_max_delay_sec,
      q_min_size,
      q_max_size,
      num_reader,
      dump_record_prefix,
      use_mcts_ai2,
      preload_sgf,
      preload_sgf_move_to,
      komi,
      print_result,
      resign_thres,
      resign_thres_lower_bound,
      resign_thres_upper_bound,
      resign_prob_never,
      resign_target_fp_rate,
      num_reset_ranking,
      ply_pass_enabled,
      following_pass,
      use_df_feature,
      policy_distri_training_for_all,
      black_use_policy_network_only,
      white_use_policy_network_only,
      cheat_eval_new_model_wins_half,
      cheat_selfplay_random_result,
      eval_num_games,
      selfplay_init_num,
      selfplay_update_num,
      selfplay_async,
      white_puct,
      white_mcts_rollout_per_batch,
      white_mcts_rollout_per_thread,
      eval_thres,
      keep_prev_selfplay,
      expected_num_clients);
};
