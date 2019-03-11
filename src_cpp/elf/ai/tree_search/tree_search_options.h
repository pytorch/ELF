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

#include "tree_search_base.h"

#include "elf/utils/json_utils.h"
#include "elf/utils/reflection.h"

namespace elf {
namespace ai {
namespace tree_search {

DEF_STRUCT(SearchAlgoOptions)

DEF_FIELD(float, c_puct, 1.0f, "PUCT value, < 0 means no c_puct is used.");
DEF_FIELD(
    bool,
    unexplored_q_zero,
    false,
    "Intentionally set all unexplored Q(s, a) = 0");
DEF_FIELD(
    bool,
    root_unexplored_q_zero,
    false,
    "Intentially set all root explored Q(s, a) = 0");

std::string info() const {
  std::stringstream ss;
  if (c_puct < 0) {
    ss << "[c_puct=" << c_puct << "]";
  }
  ss << "[uqz=" << unexplored_q_zero << "][r_uqz=" << root_unexplored_q_zero
     << "]";
  return ss.str();
}

void setJsonFields(json& j) const {
  JSON_SAVE(j, c_puct);
  JSON_SAVE(j, unexplored_q_zero);
  JSON_SAVE(j, root_unexplored_q_zero);
}

static SearchAlgoOptions createFromJson(const json& j) {
  SearchAlgoOptions opt;
  JSON_LOAD(opt, j, c_puct);
  JSON_LOAD(opt, j, unexplored_q_zero);
  JSON_LOAD(opt, j, root_unexplored_q_zero);
  return opt;
}

friend bool operator==(
    const SearchAlgoOptions& t1,
    const SearchAlgoOptions& t2) {
  if (t1.c_puct != t2.c_puct)
    return false;
  if (t1.unexplored_q_zero != t2.unexplored_q_zero)
    return false;
  if (t1.root_unexplored_q_zero != t2.root_unexplored_q_zero)
    return false;
  return true;
}

DEF_END

DEF_STRUCT(TSOptions)
DEF_FIELD(
    int,
    max_num_move,
    0,
    "Max number of moves allowed (maybe deprecated)");
DEF_FIELD(int, num_thread, 16, "#MCTS threads");
DEF_FIELD(int, num_rollout_per_thread, 100, "#rollouts per thread");
DEF_FIELD(int, num_rollout_per_batch, 8, "#rollouts per batch");
DEF_FIELD(bool, verbose, false, "MCTS Verbose");
DEF_FIELD(bool, verbose_time, false, "MCTS VerboseTime");
DEF_FIELD(long, seed, 0, "MCTS seed");
DEF_FIELD(bool, persistent_tree, false, "Use Persistent Tree");
DEF_FIELD(float, root_epsilon, 0.0f, "Dirichlet epsilon");
DEF_FIELD(float, root_alpha, 0.0f, "Dirichlet alpha");
DEF_FIELD(std::string, log_prefix, "", "If nonempty, prefix of MCTS logging");
DEF_FIELD(
    int,
    time_sec_allowed_per_move,
    -1,
    "Time (in sec) allowed in each move");
DEF_FIELD(bool, ponder, false, "Pondering");

// [TODO] Not a good design.
// string pick_method = "strongest_prior";
DEF_FIELD(
    std::string,
    pick_method,
    "most_visited",
    "Ways to pick final MCTS moves (most_visited, strongest_prior, uniform)");

DEF_FIELD(float, discount_factor, 1.0f, "Discount Factor");
DEF_FIELD_NODEFAULT(SearchAlgoOptions, alg_opt, "MCTS Algorithm Options");

// Pre-added pseudo playout.
DEF_FIELD(float, virtual_loss, 0.0f, "Virtual loss");

std::string info(bool verbose = false) const {
  std::stringstream ss;

  if (verbose) {
    ss << "Maximal #moves (0 = no constraint): " << max_num_move << std::endl;
    ss << "Seed: " << seed << std::endl;
    ss << "Log Prefix: " << log_prefix << std::endl;
    ss << "#Threads: " << num_thread << std::endl;
    ss << "#Rollout per thread: " << num_rollout_per_thread
       << ", #rollouts per batch: " << num_rollout_per_batch << std::endl;
    ss << "Verbose: " << elf_utils::print_bool(verbose)
       << ", Verbose_time: " << elf_utils::print_bool(verbose_time)
       << std::endl;
    ss << "Persistent tree: " << elf_utils::print_bool(persistent_tree)
       << std::endl;
    ss << "#Virtual loss: " << virtual_loss << std::endl;
    ss << "Discount factor: " << discount_factor << std::endl;
    ss << "Pick method: " << pick_method << std::endl;

    if (root_epsilon > 0) {
      ss << "Root exploration: epsilon: " << root_epsilon
         << ", alpha: " << root_alpha << std::endl;
    }
    ss << "Algorithm: " << alg_opt.info() << std::endl;

  } else {
    ss << "[#th=" << num_thread << "][rl=" << num_rollout_per_thread
       << "][per=" << persistent_tree << "][eps=" << root_epsilon
       << "][alpha=" << root_alpha << "]" << alg_opt.info();
  }

  return ss.str();
}

friend bool operator==(const TSOptions& t1, const TSOptions& t2) {
  if (t1.max_num_move != t2.max_num_move) {
    return false;
  }
  if (t1.num_thread != t2.num_thread) {
    return false;
  }
  if (t1.num_rollout_per_thread != t2.num_rollout_per_thread) {
    return false;
  }
  if (t1.num_rollout_per_batch != t2.num_rollout_per_batch) {
    return false;
  }
  if (t1.verbose != t2.verbose) {
    return false;
  }
  if (t1.verbose_time != t2.verbose_time) {
    return false;
  }
  if (t1.seed != t2.seed) {
    return false;
  }
  if (t1.persistent_tree != t2.persistent_tree) {
    return false;
  }
  if (t1.pick_method != t2.pick_method) {
    return false;
  }
  if (t1.log_prefix != t2.log_prefix) {
    return false;
  }
  if (t1.root_epsilon != t2.root_epsilon) {
    return false;
  }
  if (t1.root_alpha != t2.root_alpha) {
    return false;
  }
  if (!(t1.alg_opt == t2.alg_opt)) {
    return false;
  }
  if (t1.virtual_loss != t2.virtual_loss) {
    return false;
  }
  if (t1.discount_factor != t2.discount_factor) {
    return false;
  }
  return true;
}

void setJsonFields(json& j) const {
  JSON_SAVE(j, max_num_move);
  JSON_SAVE(j, num_thread);
  JSON_SAVE(j, num_rollout_per_thread);
  JSON_SAVE(j, num_rollout_per_batch);
  JSON_SAVE(j, verbose);
  JSON_SAVE(j, verbose_time);
  JSON_SAVE(j, seed);
  JSON_SAVE(j, persistent_tree);
  JSON_SAVE(j, pick_method);
  JSON_SAVE(j, log_prefix);
  JSON_SAVE(j, root_epsilon);
  JSON_SAVE(j, root_alpha);
  JSON_SAVE(j, virtual_loss);
  JSON_SAVE(j, discount_factor);
  JSON_SAVE_OBJ(j, alg_opt);
}

static TSOptions createFromJson(const json& j) {
  TSOptions opt;
  JSON_LOAD(opt, j, max_num_move);
  JSON_LOAD(opt, j, num_thread);
  JSON_LOAD(opt, j, num_rollout_per_thread);
  JSON_LOAD(opt, j, num_rollout_per_batch);
  JSON_LOAD(opt, j, verbose);
  JSON_LOAD(opt, j, verbose_time);
  JSON_LOAD(opt, j, seed);
  JSON_LOAD(opt, j, persistent_tree);
  JSON_LOAD(opt, j, pick_method);
  JSON_LOAD(opt, j, log_prefix);
  JSON_LOAD(opt, j, root_epsilon);
  JSON_LOAD(opt, j, root_alpha);
  JSON_LOAD(opt, j, virtual_loss);
  JSON_LOAD(opt, j, discount_factor);
  JSON_LOAD_OBJ(opt, j, alg_opt);
  return opt;
}

DEF_END

DEF_STRUCT(CtrlOptions)
  DEF_FIELD(int64_t, msec_start_time, -1, "Timestamp when receiving command (e.g., genmove), -1 means invalid");
  DEF_FIELD(int64_t, msec_time_left, -1, "Time left (in msec) in the match, -1 means invalid");
  DEF_FIELD(int64_t, byoyomi, -1, "Byoyomi count. -1 means invalid.");
  DEF_FIELD(int64_t, rollout_per_thread, -1, "Set #rollout per thread. -1 means invalid");
  DEF_FIELD(int64_t, msec_per_move, -1, "Specified time spent (in msec) per move");

  std::string info() const {
    std::stringstream ss;
    ss << "MCTSCtrlOptions: msec_start_time: " << msec_start_time; 
    if (msec_time_left > 0) ss << ", time_left = " << msec_time_left << " msec"; 
    if (byoyomi > 0) ss << ", byoyomi = " << byoyomi;
    if (rollout_per_thread > 0) ss << ", rollout_per_thread = " << rollout_per_thread;
    if (msec_per_move > 0) ss << ", msec_per_move = " << msec_per_move << " msec";
    return ss.str();
  }

  void reset() {
    msec_start_time = -1;
    msec_time_left = -1;
    byoyomi = -1;
    rollout_per_thread = -1;
    msec_per_move = -1;
  }

  void append(const CtrlOptions &options) {
    if (options.msec_start_time > 0) {
      msec_start_time = options.msec_start_time;
    }
    if (options.msec_time_left > 0) {
      msec_time_left = options.msec_time_left;
    }
    if (options.byoyomi > 0) {
      byoyomi = options.byoyomi;
    }
    if (options.rollout_per_thread > 0) {
      rollout_per_thread = options.rollout_per_thread;
    }
    if (options.msec_per_move > 0) {
      msec_per_move = options.msec_per_move;
    }
  }

DEF_END


} // namespace tree_search
} // namespace ai
} // namespace elf

namespace std {

template <>
struct hash<elf::ai::tree_search::SearchAlgoOptions> {
  typedef elf::ai::tree_search::SearchAlgoOptions argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    result_type const h1(std::hash<float>{}(s.c_puct));
    result_type const h2(std::hash<bool>{}(s.unexplored_q_zero));
    result_type const h3(std::hash<bool>{}(s.root_unexplored_q_zero));
    // [TODO] Not a good combination..we might need to try something different.
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

template <>
struct hash<elf::ai::tree_search::TSOptions> {
  typedef elf::ai::tree_search::TSOptions argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    // TODO: We need to fix this.
    result_type const h1(std::hash<int>{}(s.max_num_move));
    result_type const h2(std::hash<int>{}(s.num_thread));
    result_type const h3(std::hash<int>{}(s.num_rollout_per_thread));
    result_type const h4(std::hash<int>{}(s.seed));
    result_type const h5(std::hash<bool>{}(s.persistent_tree));
    result_type const h6(std::hash<std::string>{}(s.pick_method));
    result_type const h7(std::hash<float>{}(s.root_epsilon));
    result_type const h8(std::hash<float>{}(s.root_alpha));
    result_type const h9(
        std::hash<elf::ai::tree_search::SearchAlgoOptions>{}(s.alg_opt));
    // [TODO] Not a good combination..we might need to try something different.
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^
        (h7 << 6) ^ (h8 << 7) ^ (h9 << 8);
  }
};

}; // namespace std
