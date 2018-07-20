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

#include "elf/legacy/pybind_helper.h"
#include "elf/utils/json_utils.h"

namespace elf {
namespace ai {
namespace tree_search {

struct SearchAlgoOptions {
  bool use_prior = true;
  float c_puct = 5;
  bool unexplored_q_zero = false;
  bool root_unexplored_q_zero = false;

  std::string info() const {
    std::stringstream ss;
    ss << "[prior=" << use_prior << "]";
    if (use_prior) {
      ss << "[c_puct=" << c_puct << "]";
    }
    ss << "[uqz=" << unexplored_q_zero << "][r_uqz=" << root_unexplored_q_zero
       << "]";
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, use_prior);
    JSON_SAVE(j, c_puct);
    JSON_SAVE(j, unexplored_q_zero);
    JSON_SAVE(j, root_unexplored_q_zero);
  }

  static SearchAlgoOptions createFromJson(const json& j) {
    SearchAlgoOptions opt;
    JSON_LOAD(opt, j, use_prior);
    JSON_LOAD(opt, j, c_puct);
    JSON_LOAD(opt, j, unexplored_q_zero);
    JSON_LOAD(opt, j, root_unexplored_q_zero);
    return opt;
  }

  friend bool operator==(
      const SearchAlgoOptions& t1,
      const SearchAlgoOptions& t2) {
    if (t1.use_prior != t2.use_prior)
      return false;
    if (t1.c_puct != t2.c_puct)
      return false;
    if (t1.unexplored_q_zero != t2.unexplored_q_zero)
      return false;
    if (t1.root_unexplored_q_zero != t2.root_unexplored_q_zero)
      return false;
    return true;
  }

  REGISTER_PYBIND_FIELDS(
      use_prior,
      c_puct,
      unexplored_q_zero,
      root_unexplored_q_zero);
};

struct TSOptions {
  int max_num_moves = 0;
  int num_threads = 16;
  int num_rollouts_per_thread = 100;
  int num_rollouts_per_batch = 8;
  bool verbose = false;
  bool verbose_time = false;
  int seed = 0;
  bool persistent_tree = false;
  float root_epsilon = 0.0;
  float root_alpha = 0.0;
  std::string log_prefix = "";

  // [TODO] Not a good design.
  // string pick_method = "strongest_prior";
  std::string pick_method = "most_visited";

  SearchAlgoOptions alg_opt;

  // Pre-added pseudo playout.
  int virtual_loss = 0;

  std::string info(bool verbose = false) const {
    std::stringstream ss;

    if (verbose) {
      ss << "Maximal #moves (0 = no constraint): " << max_num_moves
         << std::endl;
      ss << "Seed: " << seed << std::endl;
      ss << "Log Prefix: " << log_prefix << std::endl;
      ss << "#Threads: " << num_threads << std::endl;
      ss << "#Rollout per thread: " << num_rollouts_per_thread
         << ", #rollouts per batch: " << num_rollouts_per_batch << std::endl;
      ss << "Verbose: " << elf_utils::print_bool(verbose)
         << ", Verbose_time: " << elf_utils::print_bool(verbose_time)
         << std::endl;
      ss << "Persistent tree: " << elf_utils::print_bool(persistent_tree)
         << std::endl;
      ss << "#Virtual loss: " << virtual_loss << std::endl;
      ss << "Pick method: " << pick_method << std::endl;

      if (root_epsilon > 0) {
        ss << "Root exploration: epsilon: " << root_epsilon
           << ", alpha: " << root_alpha << std::endl;
      }
      ss << "Algorithm: " << alg_opt.info() << std::endl;

    } else {
      ss << "[#th=" << num_threads << "][rl=" << num_rollouts_per_thread
         << "][per=" << persistent_tree << "][eps=" << root_epsilon
         << "][alpha=" << root_alpha << "]" << alg_opt.info();
    }

    return ss.str();
  }

  friend bool operator==(const TSOptions& t1, const TSOptions& t2) {
    if (t1.max_num_moves != t2.max_num_moves) {
      return false;
    }
    if (t1.num_threads != t2.num_threads) {
      return false;
    }
    if (t1.num_rollouts_per_thread != t2.num_rollouts_per_thread) {
      return false;
    }
    if (t1.num_rollouts_per_batch != t2.num_rollouts_per_batch) {
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
    return true;
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, max_num_moves);
    JSON_SAVE(j, num_threads);
    JSON_SAVE(j, num_rollouts_per_thread);
    JSON_SAVE(j, num_rollouts_per_batch);
    JSON_SAVE(j, verbose);
    JSON_SAVE(j, verbose_time);
    JSON_SAVE(j, seed);
    JSON_SAVE(j, persistent_tree);
    JSON_SAVE(j, pick_method);
    JSON_SAVE(j, log_prefix);
    JSON_SAVE(j, root_epsilon);
    JSON_SAVE(j, root_alpha);
    JSON_SAVE(j, virtual_loss);
    JSON_SAVE_OBJ(j, alg_opt);
  }

  static TSOptions createFromJson(const json& j) {
    TSOptions opt;
    JSON_LOAD(opt, j, max_num_moves);
    JSON_LOAD(opt, j, num_threads);
    JSON_LOAD(opt, j, num_rollouts_per_thread);
    JSON_LOAD(opt, j, num_rollouts_per_batch);
    JSON_LOAD(opt, j, verbose);
    JSON_LOAD(opt, j, verbose_time);
    JSON_LOAD(opt, j, seed);
    JSON_LOAD(opt, j, persistent_tree);
    JSON_LOAD(opt, j, pick_method);
    JSON_LOAD(opt, j, log_prefix);
    JSON_LOAD(opt, j, root_epsilon);
    JSON_LOAD(opt, j, root_alpha);
    JSON_LOAD(opt, j, virtual_loss);
    JSON_LOAD_OBJ(opt, j, alg_opt);
    return opt;
  }

  REGISTER_PYBIND_FIELDS(
      max_num_moves,
      num_threads,
      num_rollouts_per_thread,
      num_rollouts_per_batch,
      verbose,
      persistent_tree,
      pick_method,
      log_prefix,
      virtual_loss,
      verbose_time,
      alg_opt,
      root_epsilon,
      root_alpha);
};

} // namespace tree_search
} // namespace ai
} // namespace elf

namespace std {

template <>
struct hash<elf::ai::tree_search::SearchAlgoOptions> {
  typedef elf::ai::tree_search::SearchAlgoOptions argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    result_type const h1(std::hash<bool>{}(s.use_prior));
    result_type const h2(std::hash<float>{}(s.c_puct));
    result_type const h3(std::hash<bool>{}(s.unexplored_q_zero));
    result_type const h4(std::hash<bool>{}(s.root_unexplored_q_zero));
    // [TODO] Not a good combination..we might need to try something different.
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
  }
};

template <>
struct hash<elf::ai::tree_search::TSOptions> {
  typedef elf::ai::tree_search::TSOptions argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    result_type const h1(std::hash<int>{}(s.max_num_moves));
    result_type const h2(std::hash<int>{}(s.num_threads));
    result_type const h3(std::hash<int>{}(s.num_rollouts_per_thread));
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
