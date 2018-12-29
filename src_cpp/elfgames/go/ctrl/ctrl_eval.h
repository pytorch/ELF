/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include "ctrl_utils.h"
#include "elf/ai/tree_search/tree_search_options.h"
#include "elf/utils/utils.h"

#include "../state/go_game_specific.h"
#include "stats/fair_pick.h"

using TSOptions = elf::ai::tree_search::TSOptions;

constexpr int CLIENT_SELFPLAY_ONLY = 0;
constexpr int CLIENT_EVAL_THEN_SELFPLAY = 1;

class ModelPerf {
 public:
  enum EvalResult {
    EVAL_INVALID,
    EVAL_INCOMPLETE,
    EVAL_BLACK_PASS,
    EVAL_BLACK_NOTPASS
  };

  ModelPerf(const GameOptionsTrain& options, const ModelPair& p)
      : options_(options), curr_pair_(p) {
    const size_t cushion = 5;
    const size_t max_request_per_layer = options.expected_eval_clients / 3;
    const size_t num_request = options.eval_num_games / 2 + cushion;
    const size_t num_eval_machine_per_layer =
        compute_num_eval_machine(num_request, max_request_per_layer);

    games_.reset(new fair_pick::Pick(num_request, num_eval_machine_per_layer));
    swap_games_.reset(
        new fair_pick::Pick(num_request, num_eval_machine_per_layer));
    record_.resetPrefix(
        eval_prefix() + "-" + std::to_string(p.black_ver) + "-" +
        std::to_string(p.white_ver));
  }

  ModelPerf(ModelPerf&&) = default;

  int n_done() const {
    return games_->win_count().n_done() + swap_games_->win_count().n_done();
  }
  int n_win() const {
    return games_->win_count().n_win() + swap_games_->win_count().n_win();
  }

  float winrate() const {
    const int num_games = n_done();
    return num_games == 0 ? 0.0 : static_cast<float>(n_win()) / num_games;
  }

  EvalResult eval_result() const {
    return eval_result_;
  }

  std::string info() const {
    std::stringstream ss;
    ss << curr_pair_.info() << ", overall_wr: " << winrate() << "/" << n_done()
       << ", s/v: " << sent_ << "/" << recv_ << ", sealed: " << sealed_ << ", "
       << "|| Noswap: " << games_->info() << "|| Swap: " << swap_games_->info();
    return ss.str();
  }

  EvalResult updateState(fair_pick::IsStuckFunc is_stuck_func) {
    if (sealed_)
      return eval_result_;

    games_->checkStuck(is_stuck_func);
    swap_games_->checkStuck(is_stuck_func);

    eval_result_ = eval_check();

    if (n_done() > 0 && (sent_ % 100 == 0 || recv_ % 100 == 0)) {
      std::cout << "EvalResult: [" << elf_utils::now() << "]" << info()
                << std::endl;
    }

    if (sealed_ || eval_result_ == EVAL_INCOMPLETE)
      return eval_result_;

    set_sealed();
    return eval_result_;
  }

  void feed(const std::string &client_key, const Request &request, const Result &result, const Record &r) {
    if (request.player_swap) {
      swap_games_->add(client_key, -result.reward);
    } else {
      games_->add(client_key, result.reward);
    }
    record_.feed(r);
    recv_++;
  }

  void fillInRequest(const std::string &k, Request* msg) {
    if (sealed_)
      return;

    // decide order by checking the number of games.
    std::pair<fair_pick::Pick*, bool> games[2] = {{games_.get(), false},
                                                  {swap_games_.get(), true}};

    if (games_->n_reg_to_go() < swap_games_->n_reg_to_go()) {
      swap(games[0], games[1]);
    }

    for (const auto& g : games) {
      fair_pick::RegisterResult res = g.first->reg(k);
      // cout << "a = " << a << ", a_swap: " << a_swap << endl;
      if (fair_pick::release_request(res))
        continue;
      if (sent_ % 100 == 0) {
        std::cout << elf_utils::now()
                  << " Sending evaluation request: " << curr_pair_.info()
                  << ", sent: " << sent_ << std::endl;
      }

      // We only use eval_num_threads threads to run evaluation to make it
      // faster.
      msg->vers = curr_pair_;
      // Now treat player_swap as same as other quantities.
      msg->player_swap = g.second;
      msg->resign_thres = options_.resign_thres;
      msg->num_game_thread_used = options_.eval_num_threads;
      break;
    }
    sent_++;
  }

  bool IsSealed() const {
    return sealed_;
  }
  const ModelPair& Pair() const {
    return curr_pair_;
  }

 private:
  const GameOptionsTrain& options_;
  const ModelPair curr_pair_;

  // For each machine + game_id, the list of rewards.
  // Note that game_id decides whether we swap the player or not.
  std::unique_ptr<fair_pick::Pick> games_, swap_games_;

  int sent_ = 0, recv_ = 0;
  bool sealed_ = false;
  RecordBuffer record_;
  EvalResult eval_result_ = EVAL_INVALID;

  static size_t compute_num_eval_machine(size_t n, size_t max_num_eval) {
    if (max_num_eval == 0)
      return 1;

    // if n = 200, max_num_eval = 150, then min_pass = 2, and return 100
    // if n = 200, max_num_eval = 50, then min_pass = 4, and return 50
    size_t min_pass = (n + max_num_eval - 1) / max_num_eval;

    size_t num_eval = (n + min_pass - 1) / min_pass;
    return std::min(num_eval, max_num_eval);
  }

  std::string eval_prefix() const {
    return "eval-" + options_.common.net.server_id + "-" +
        options_.common.base.time_signature;
  }

  EvalResult eval_check() const {
    const int half_complete = options_.eval_num_games / 2;
    const float wr = winrate();

    const auto& report = games_->win_count();
    const auto& swap_report = swap_games_->win_count();

    if (report.n_done() >= half_complete &&
        swap_report.n_done() >= half_complete) {
      return wr >= options_.eval_thres ? EVAL_BLACK_PASS : EVAL_BLACK_NOTPASS;
    }
    /*
    auto res = report.CheckWinrateBound(half_complete, options_.eval_thres);
    auto swap_res = swap_report.CheckWinrateBound(half_complete,
    options_.eval_thres);

    if (res == fair_pick::LOSS && swap_res == fair_pick::LOSS) {
      return EVAL_BLACK_NOTPASS;
    }
    if (res == fair_pick::WIN && swap_res == fair_pick::WIN) {
      return EVAL_BLACK_PASS;
    }
    */

    return EVAL_INCOMPLETE;
  }

  void set_sealed() {
    // Save all games.
    sealed_ = true;
    std::cout << "Sealed[pass=" << (eval_result_ == EVAL_BLACK_PASS) << "]["
              << elf_utils::now() << "]" << info() << ", "
              << record_.prefix_save_counter() << std::endl;
    record_.saveCurrent();
    record_.clear();
  }
};

class EvalSubCtrl {
 public:
  EvalSubCtrl(const GameOptionsTrain& options) : options_(options) {
    // [TODO]: A bit hacky, we need to have a better way for this.
    options_.common.mcts.alg_opt.unexplored_q_zero = false;
    options_.common.mcts.alg_opt.root_unexplored_q_zero = false;
    options_.common.mcts.root_epsilon = 0.0;
    options_.common.mcts.root_alpha = 0.0;
  }

  int64_t updateState(fair_pick::IsStuckFunc is_stuck_func) {
    // Note that models_to_eval_ might change during the loop.
    // So we need to make a copy.
    std::lock_guard<std::mutex> lock(mutex_);
    auto models_to_eval = models_to_eval_;

    for (const auto& ver : models_to_eval) {
      // cout << "UpdateState: checking ver = " << ver << endl;
      ModelPerf& perf = find_or_create(get_key(ver));

      auto res = perf.updateState(is_stuck_func);
      switch (res) {
        case ModelPerf::EVAL_INVALID:
          std::cout << "res cannot be EVAL_INVALID" << std::endl;
          assert(false);
        case ModelPerf::EVAL_INCOMPLETE:
          break;
        case ModelPerf::EVAL_BLACK_PASS:
          // Check whether we need to make a conclusion.
          // Update reference.
          return perf.Pair().black_ver;
        case ModelPerf::EVAL_BLACK_NOTPASS:
          // In any case, pick the next model to evaluate.
          remove_candidate_model(perf.Pair().black_ver);
          break;
      }
    }
    // No new model.
    return -1;
  }

  FeedResult feed(const std::string &k, const Request &request, const Result &result, const Record& r) {
    if (request.vers.is_selfplay())
      return NOT_EVAL;

    std::lock_guard<std::mutex> lock(mutex_);

    ModelPerf* perf = find_or_null(request.vers);
    if (perf == nullptr)
      return NOT_REQUESTED;

    perf->feed(k, request, result, r);
    return FEEDED;
  }

  int64_t getBestModel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return best_baseline_model_;
  }

  void fillInRequest(const std::string &k, Request* msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Go through all current models
    // It uses the implicit heuristic that started from the oldest
    // model first.
    // Note that on_eval_status might change models_to_eval,
    for (const auto& ver : models_to_eval_) {
      // cout << "fillInRequests, checking ver = " << ver << endl;
      ModelPerf& perf = find_or_create(get_key(ver));
      perf.fillInRequest(k, msg);
      if (!msg->vers.wait())
        break;
    }
  }

  void setBaselineModel(int64_t ver) {
    std::lock_guard<std::mutex> lock(mutex_);
    best_baseline_model_ = ver;
    models_to_eval_.clear();
    // All perfs need to go away as well.
    // perfs_.clear();
    std::cout << "Set new baseline model, ver: " << ver << std::endl;
  }

  void addNewModelForEvaluation(int64_t selfplay_ver, int64_t new_version) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (selfplay_ver == best_baseline_model_) {
      if (selfplay_ver < new_version) {
        std::cout << "Add new version: " << new_version
                  << ", selfplay_ver: " << selfplay_ver
                  << ", baseline: " << best_baseline_model_
                  << options_.common.mcts.info() << std::endl;
        add_candidate_model(new_version);
      } else {
        std::cout << "New version: " << new_version
                  << " is the same or earlier tha "
                  << ", baseline: " << best_baseline_model_
                  << options_.common.mcts.info() << std::endl;
      }
    } else {
      std::cout << "New version " << new_version << " is not registered. "
                << "Selfplay_ver " << selfplay_ver << " != internal one "
                << best_baseline_model_ << std::endl;
    }
  }

 private:
  mutable std::mutex mutex_;

  GameOptionsTrain options_;

  int64_t best_baseline_model_ = -1;
  std::vector<int64_t> models_to_eval_;

  std::unordered_map<ModelPair, std::unique_ptr<ModelPerf>> perfs_;

  ModelPair get_key(int ver) {
    ModelPair p;
    p.black_ver = ver;
    p.white_ver = best_baseline_model_;
    p.mcts_opt = options_.common.mcts;
    return p;
  }

  bool add_candidate_model(int ver) {
    auto it = perfs_.find(get_key(ver));
    if (it == perfs_.end())
      models_to_eval_.push_back(ver);
    return it == perfs_.end();
  }

  bool remove_candidate_model(int ver) {
    for (auto it = models_to_eval_.begin(); it != models_to_eval_.end(); ++it) {
      if (*it == ver) {
        models_to_eval_.erase(it);
        // We don't remove records in perfs_.
        return true;
      }
    }
    return false;
  }

  ModelPerf& find_or_create(const ModelPair& mp) {
    auto it = perfs_.find(mp);
    if (it == perfs_.end()) {
      auto& ptr = perfs_[mp];
      ptr.reset(new ModelPerf(options_, mp));
      return *ptr;
    }
    return *it->second;
  }

  ModelPerf* find_or_null(const ModelPair& mp) {
    auto it = perfs_.find(mp);
    if (it == perfs_.end()) {
      std::cout << "The pair " + mp.info() + " was not sent before!"
                << std::endl;
      return nullptr;
    }
    return it->second.get();
  }
};
