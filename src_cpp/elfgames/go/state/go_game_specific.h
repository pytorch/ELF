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
#include "elf/ai/tree_search/tree_search_options.h"
#include "elf/interface/options.h"
#include "elf/distributed/options.h"
#include "elf/utils/reflection.h"
#include "elf/utils/utils.h"

DEF_STRUCT(GameOptions)
DEF_FIELD(std::string, mode, "", "Game mode");
DEF_FIELD(bool, use_df_feature, false, "Use DF feature");
DEF_FIELD(float, komi, 7.5f, "Komi");

DEF_FIELD_NODEFAULT(elf::msg::Options, net, "Network options");
DEF_FIELD_NODEFAULT(elf::Options, base, "Base Options");
DEF_FIELD_NODEFAULT(elf::ai::tree_search::TSOptions, mcts, "MCTS Options");
DEF_END

DEF_STRUCT(GameOptionsSelfPlay)
DEF_FIELD_NODEFAULT(GameOptions, common, "Common options");

DEF_FIELD(
    int,
    num_game_per_thread,
    -1,
    "#Games to be played per thread. -1 is infinite.");

// Use mcts engine.
DEF_FIELD(bool, use_mcts, false, "Use MCTS in AI");
DEF_FIELD(bool, use_mcts_ai2, false, "Use MCTS in AI2");

// Specify which side uses policy network only.
DEF_FIELD(
    bool,
    black_use_policy_network_only,
    false,
    "Black only uses PolicyNet");
DEF_FIELD(
    bool,
    white_use_policy_network_only,
    false,
    "White only uses PolicyNet");

DEF_FIELD(
    int,
    move_cutoff,
    -1,
    "Cutoff ply for each loaded game, and start a new one");
DEF_FIELD(int, policy_distri_cutoff, 20, "Cutoff ply for mcts policy / best a");

DEF_FIELD(int, num_reset_ranking, 5000, "#reset ranking");

DEF_FIELD(std::string, preload_sgf, "", "Preloaded SGF file");
DEF_FIELD(int, preload_sgf_move_to, -1, "Starting ply for preloaded SGF file");

DEF_FIELD(int, ply_pass_enabled, 0, "Allow pass after >= ply");

// Second puct used for ai2, if -1 then use the same puct.
DEF_FIELD(
    float,
    white_puct,
    -1.0f,
    "Different PUCT for white, -1.0 means same as black");
DEF_FIELD(
    int,
    white_mcts_rollout_per_batch,
    -1,
    "Different rollout_per_batch for white, -1 means same as black");
DEF_FIELD(
    int,
    white_mcts_rollout_per_thread,
    -1,
    "Different rollout_per_thread for white, -1 means same as black");

DEF_FIELD(
    bool,
    following_pass,
    false,
    "Online mode, if human pass, we also pass.");
DEF_FIELD(
    bool,
    policy_distri_training_for_all,
    false,
    "Save MCTS policy distribution");

DEF_FIELD(
    bool,
    cheat_eval_new_model_wins_half,
    false,
    "Cheat mode: New model gets 100% win rate half of the time");
DEF_FIELD(
    bool,
    cheat_selfplay_random_result,
    false,
    "Cheat selfplay mode: Random game outcome");

DEF_FIELD(
    std::string,
    dump_record_prefix,
    "",
    "If not empty, the file prefix used to dump game record");

DEF_END

DEF_STRUCT(GameOptionsTrain)
DEF_FIELD_NODEFAULT(GameOptions, common, "Common options");

DEF_FIELD(int, num_future_actions, 1, "Use #future actions to train PolicyNet");

DEF_FIELD(int, data_aug, -1, "-1 is random, 0-7 mean specific data aug.");

DEF_FIELD(
    int,
    q_min_size,
    10,
    "Minimal Queue size for each reader in replay buffer");
DEF_FIELD(
    int,
    q_max_size,
    1000,
    "Maximal Queue size for each reader in replay buffer");
DEF_FIELD(int, num_reader, 50, "#readers in replay buffer");

DEF_FIELD(float, resign_thres, 0.05f, "Resign threshold");
DEF_FIELD(
    float,
    resign_thres_lower_bound,
    1e-9f,
    "Lower bound of resign threshold");
DEF_FIELD(
    float,
    resign_thres_upper_bound,
    0.5f,
    "Upper bound of resign threshold");
DEF_FIELD(float, resign_prob_never, 0.1f, "Never resign probability");
DEF_FIELD(
    float,
    resign_target_fp_rate,
    0.05f,
    "Never resign false positive ratio");
DEF_FIELD(
    int,
    resign_target_hist_size,
    2500,
    "Resign history size (to determine resign threshold)");

DEF_FIELD(
    int,
    eval_num_games,
    400,
    "In sync mode (AGZ), #games used to evaluate a new model compared to the "
    "current");
DEF_FIELD(
    float,
    eval_thres,
    0.55f,
    "In sync mode (AGZ), winrate thres to acknowledge the new model is better");
DEF_FIELD(
    int,
    expected_eval_clients,
    205,
    "In sync mode (AGZ), expected number of evaluation clients");

DEF_FIELD(
    int,
    client_max_delay_sec,
    1200,
    "Maximal allowable delay for each client (in sec). If exceeded, the client "
    "is regarded as dead");

DEF_FIELD(
    int,
    selfplay_init_num,
    5000,
    "In sync mode (AGZ), initial number of selfplay games for each model used "
    "for selfplay.");
DEF_FIELD(
    int,
    selfplay_update_num,
    1000,
    "Additive number of selfplay every time the new model is updated.");

DEF_FIELD(
    bool,
    selfplay_async,
    false,
    "Using async mode for selfplay (multiple models are involved in a single "
    "selfplay game)");

DEF_FIELD(
    bool,
    keep_prev_selfplay,
    true,
    "In sync mode (AGZ), keep old self-plays when an update model is used");

DEF_FIELD(
    int,
    eval_num_threads,
    1,
    "In sync mode (AGZ), #eval threads for each client");
DEF_FIELD(
    int,
    expected_num_clients,
    -1,
    "In sync mode (AGZ), #Expected number of clients to connect to the server");

DEF_FIELD_NODEFAULT(
    std::vector<std::string>,
    list_files,
    "A list of replay files (in jsons) to load");

DEF_END
