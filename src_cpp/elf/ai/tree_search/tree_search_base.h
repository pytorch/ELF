/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "elf/ai/tree_search/tree_search_edgeinfo.h"
#include "elf/utils/utils.h"

using json = nlohmann::json;

namespace elf {
namespace ai {
namespace tree_search {

template <typename Action>
struct _NodeResponseT {
  std::unordered_map<Action, EdgeInfo> pi;
  float value = 0.0;
  bool q_flip = false;

  void normalize() {
    float total_prob = 1e-10;
    for (const auto& p : pi) {
      total_prob += p.second.prior_probability;
    }

    for (auto& p : pi) {
      p.second.prior_probability /= total_prob;
    }
  }

  std::string info() const {
    std::stringstream ss;
    ss << "value=" << value << ", q_flip=" << q_flip;
    return ss.str();
  }

  void enhanceExploration(float epsilon, float alpha, std::mt19937* rng) {
    // Note that this is not thread-safe.
    // It should be called once and only once for each node.
    if (epsilon == 0.0) {
      return;
    }

    std::gamma_distribution<> dis(alpha);

    // Draw distribution.
    std::vector<float> etas(pi.size());
    float Z = 1e-10;
    for (size_t i = 0; i < pi.size(); ++i) {
      etas[i] = dis(*rng);
      Z += etas[i];
    }

    int i = 0;
    for (auto& p : pi) {
      p.second.prior_probability =
          (1 - epsilon) * p.second.prior_probability + epsilon * etas[i] / Z;
      i++;
    }
  }

  virtual void clear() {
    pi.clear();
    value = 0.0;
    q_flip = 0.0;
  }
};

template <typename Action, typename Info>
struct NodeResponseT : public _NodeResponseT<Action> {
  std::unique_ptr<Info> info;
  void clear() override { 
    _NodeResponseT<Action>::clear(); 
    info.reset(); 
  }
};

template <typename Action>
struct NodeResponseT<Action, void> : public _NodeResponseT<Action> {};

template <typename S, typename A>
using ForwardFuncT = std::function<bool(const S& s, const A& a, S* next)>;

template <typename S>
using VisitFuncT = std::function<bool(S* s)>;

template <typename S>
using EvalFuncT = std::function<float(const S& s)>;

// TODO" Do we need StateTrait, ActionTrait, ActorTrait (ssengupta@fb)
// The following can be partial templated.
template <typename S, typename A>
struct StateTrait {
 public:
  static std::string to_string(const S&) {
    return "";
  }

  static bool equals(const S& s1, const S& s2) {
    return s1 == s2;
  }

  static bool
  moves_since(const S& /*s*/, const S& /*s_ref*/, std::vector<A>* /*moves*/) {
    // By default it is not provided.
    return false;
  }
};

template <typename Action>
struct ActionTrait {
 public:
  static std::string to_string(const Action& action) {
    return std::to_string(action);
  }

  // We expect static A default_value().
  static Action default_value() {
    return Action();
  }
};

template <typename Actor>
struct ActorTrait {
 public:
  static std::string to_string(const Actor&) {
    return "";
  }
};

template <typename Action>
struct MCTSPolicy {
  std::vector<std::pair<Action, float>> policy;

  std::string info() const {
    std::stringstream ss;
    ss << "Printing out scores for each action." << std::endl;
    for (size_t i = 0; i < policy.size(); i++) {
      const auto& entry = policy[i];
      ss << "A: " << entry.first << ", Score: " << entry.second << std::endl;
    }
    return ss.str();
  }

  void addAction(const Action& action, float score) {
    policy.push_back(std::make_pair(action, score));
  }

  // AlphaGo use t=1 for first 30 moves.
  void normalize(float t = 1) {
    float exp_sum = 0;
    for (auto& entry : policy) {
      float e = std::pow(entry.second, 1.0 / t);
      entry.second = e;
      exp_sum += e;
    }
    for (auto& entry : policy) {
      entry.second /= exp_sum;
    }
  }

  // Sample from the distribution.
  Action sampleAction(std::mt19937* gen) const {
    size_t i = elf_utils::sample_multinomial(policy, gen);
    return policy[i].first;
  }
};

template <typename Action>
struct MCTSResultT {
  enum RankCriterion { MOST_VISITED = 0, PRIOR = 1, UNIFORM_RANDOM };

  Action best_action = ActionTrait<Action>::default_value();
  float root_value = 0.0;
  float max_score = std::numeric_limits<float>::lowest();
  EdgeInfo best_edge_info;
  MCTSPolicy<Action> mcts_policy;
  std::vector<std::pair<Action, EdgeInfo>> action_edge_pairs;
  int total_visits = 0;
  RankCriterion action_rank_method = MOST_VISITED;

  MCTSResultT() : best_edge_info(0) {}

  // TODO: This function should be private and called from the constructor
  //       ssengupta@fb.com
  MCTSResultT(RankCriterion rc, const _NodeResponseT<Action>& resp)
      : best_edge_info(0) {
    action_rank_method = rc;
    static std::mt19937 rng(time(NULL));
    int random_idx = 0;

    assert(resp.pi.size() > 0);

    if (action_rank_method == UNIFORM_RANDOM) {
      random_idx = rng() % resp.pi.size();
    }

    int index = 0;
    for (const std::pair<Action, EdgeInfo>& action_edge : resp.pi) {
      // float score = 0;

      float score = (action_rank_method == MOST_VISITED)
          ? action_edge.second.num_visits
          : (action_rank_method == PRIOR) ? action_edge.second.prior_probability
                                          : 1;

#if 0
      switch (action_rank_method) {
        case MOST_VISITED:
          score = action_edge.second.num_visits;
          break;
        case PRIOR:
          score = action_edge.second.prior_probability;
          break;
        case UNIFORM_RANDOM:
          score = 1;
          break;
        default:
          break;
      }
#endif

      mcts_policy.addAction(action_edge.first, score);
      action_edge_pairs.push_back(action_edge);
      total_visits += action_edge.second.num_visits;

      if (action_rank_method == UNIFORM_RANDOM) {
        // Choose random action
        if (index == random_idx) {
          max_score = score;
          best_action = action_edge.first;
          best_edge_info = action_edge.second;
        }
      } else {
        // Choose action with max score
        if (score > max_score) {
          max_score = score;
          best_action = action_edge.first;
          best_edge_info = action_edge.second;
        }
      }

      index++;
    }
    root_value = resp.value;
  }

#if 0
  bool feed(float score, const std::pair<Action, EdgeInfo>& action_edge) {
    mcts_policy.addAction(action_edge.first, score);
    action_edge_pairs.push_back(action_edge);
    total_visits += action_edge.second.num_visits;

    if (score > max_score) {
      max_score = score;
      best_action = action_edge.first;
      best_edge_info = action_edge.second;
      return true;
    }
    return false;
  }
#endif

  std::vector<std::pair<Action, EdgeInfo>> getSorted() const {
    return getSorted(action_rank_method);
  }

  std::vector<std::pair<Action, EdgeInfo>> getSorted(RankCriterion rc) const {
    // TODO: This assignment is a bit odd (ssengupta@fb.com)
    auto ae_pairs = action_edge_pairs;

    using T = std::pair<Action, EdgeInfo>;
    using Func = std::function<bool(const T&, const T&)>;
    Func cmp = nullptr;
    switch (rc) {
      case MOST_VISITED: {
        cmp = [](const T& p1, const T& p2) {
          return p1.second.num_visits > p2.second.num_visits;
        };
        std::sort(ae_pairs.begin(), ae_pairs.end(), cmp);
      } break;
      case PRIOR: {
        cmp = [](const T& p1, const T& p2) {
          return p1.second.prior_probability > p2.second.prior_probability;
        };
        std::sort(ae_pairs.begin(), ae_pairs.end(), cmp);
      } break;
      case UNIFORM_RANDOM:
        break;
      default:
        break;
    }
    return ae_pairs;
  }

  std::pair<int, EdgeInfo> getRank(const Action& action, RankCriterion rc)
      const {
    // [TODO] not efficient if you want to get rank for multiple actions.
    auto ae_pairs = getSorted(rc);

    for (int i = 0; i < (int)ae_pairs.size(); ++i) {
      if (ae_pairs[i].first == action) {
        return std::make_pair(i, ae_pairs[i].second);
      }
    }
    return std::make_pair(-1, EdgeInfo(0));
  }

  std::string info() const {
    std::stringstream ss;
    ss << "BestA: " << ActionTrait<Action>::to_string(best_action)
       << ", MaxScore: " << max_score << ", Info: " << best_edge_info.info();
    return ss.str();
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
