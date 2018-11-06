/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>

#include "elf/ai/tree_search/mcts.h"
#include "elfgames/tasks/mcts/ai.h"

#define UNUSED(expr) do { (void)(expr); } while (0)

struct MCTSActorParams {
  std::string actor_name;
  int ply_pass_enabled = 0;
  uint64_t seed = 0;
  // Required model version.
  // If -1, then there is no requirement on model version (any model response
  // can be used).
  int64_t required_version = -1;
  bool remove_pass_if_dangerous = true;
  bool rotation_flip = false;
  float komi = 7.5;

  std::string info() const {
    std::stringstream ss;
    ss << "[name=" << actor_name << "][ply_pass_enabled=" << ply_pass_enabled
       << "][seed=" << seed << "][requred_ver=" << required_version
       << "][remove_pass_if_dangerous=" << remove_pass_if_dangerous
       << "][rotation_flip=" << rotation_flip << "][komi=" << komi << "]";
    return ss.str();
  }
};

class MCTSActor {
 public:
  using Action = Coord;
  using State = StateForChouFleur;
  using NodeResponse = elf::ai::tree_search::NodeResponseT<Coord>;

  enum PreEvalResult { EVAL_DONE, EVAL_NEED_NN };

  MCTSActor(elf::GameClient* client, const MCTSActorParams& params)
      : params_(params), rng_(params.seed) {
    ai_.reset(new AI(client, {params_.actor_name}));
  }

  std::string info() const {
    return params_.info();
  }

  void set_ostream(std::ostream* oo) {
    oo_ = oo;
  }

  void setRequiredVersion(int64_t ver) {
    params_.required_version = ver;
  }

  std::mt19937* rng() {
    return &rng_;
  }

  // batch evaluate.
  void evaluate(
      const std::vector<const StateForChouFleur*>& states,
      std::vector<NodeResponse>* p_resps) {
    std::cout << ("here, tasks/mcts launches a batch evaluation.") << std::endl;
    if (states.empty())
    {
      std::cout << "and returning because empty" << std::endl;
      return;
    }

    if (oo_ != nullptr)
      *oo_ << "Evaluating batch state. #states: " << states.size() << std::endl;

    auto& resps = *p_resps;

    resps.resize(states.size());
    std::vector<BoardFeature> sel_bfs;
    std::vector<size_t> sel_indices;

    for (size_t i = 0; i < states.size(); i++) {
      std::cout << "states" << i << "/" << states.size() << std::endl;
      assert(states[i] != nullptr);
      PreEvalResult res = pre_evaluate(*states[i], &resps[i]);
      if (res == EVAL_NEED_NN) {
        sel_bfs.push_back(get_extractor(*states[i]));
        sel_indices.push_back(i);
      }
    }

    if (sel_bfs.empty()) {
      std::cout << " bfs empty" << std::endl;
      return;
    }

    std::vector<ChouFleurReply> replies;
    for (size_t i = 0; i < sel_bfs.size(); ++i) {
      std::cout << "bfs" << i << "/" << sel_bfs.size() << std::endl;
      replies.emplace_back(sel_bfs[i]);
    }
    std::cout << " ok bfs" << std::endl;
    // Get all pointers.
    std::vector<ChouFleurReply*> p_replies;
    std::vector<const BoardFeature*> p_bfs;

    for (size_t i = 0; i < sel_bfs.size(); ++i) {
      p_bfs.push_back(&sel_bfs[i]);
      p_replies.push_back(&replies[i]);
    }

    std::cout << " ok replies" << std::endl;
    // cout << "About to send situation to " << params_.actor_name << endl;
    // cout << s.showBoard() << endl;
    if (!ai_->act_batch(p_bfs, p_replies)) {
      std::cout << "act unsuccessful! " << std::endl;
    } else {
      //std::cout << "act successful!" << std::endl;
      for (size_t i = 0; i < sel_indices.size(); i++) {
        std::cout << "postnnresults" << i << "/" << sel_indices.size() << std::endl;
        post_nn_result(replies[i], &resps[sel_indices[i]]);
      }
    }
   std::cout << ("here, tasks/mcts launches a batch evaluation.       ----       END") << std::endl;
  }

  void evaluate(const StateForChouFleur& s, NodeResponse* resp) {
    if (oo_ != nullptr)
      *oo_ << "Evaluating state at " << std::hex << &s << std::dec << std::endl;
    std::cout << ("evaluating a state, evaluate(.,., client side")  << std::endl;
    // if terminated(), get results, res = done
    // else res = EVAL_NEED_NN
    PreEvalResult res = pre_evaluate(s, resp);

    if (res == EVAL_NEED_NN) {
      BoardFeature bf = get_extractor(s);
      // ChouFleurReply struct initialization
      // members containing:
      // Coord c, vector<float> pi, float v;
      ChouFleurReply reply(bf);
      // cout << "About to send situation to " << params_.actor_name << endl;
      // cout << s.showBoard() << endl;

      // AI-Client will run a one-step neural network
      if (!ai_->act(bf, &reply)) {
        // This happens when the game is about to end,
        std::cout << "act unsuccessful! " << std::endl;
      } else {
        // call pi2response()
        // action will be inv-transformed
        post_nn_result(reply, resp);
      }
    }

    std::cout << ("evaluating a state, evaluate(.,., client side           END") << std::endl;
    if (oo_ != nullptr)
      *oo_ << "Finish evaluating state at " << std::hex << &s << std::dec
           << std::endl;
  }

  bool forward(StateForChouFleur& s, Coord a) {
    std::cout << " mcts forward " << std::endl;
    return s.forward(a);
  }

  void setID(int id) {
    ai_->setID(id);
  }

  float reward(const StateForChouFleur& /*s*/, float value) const {
    return value;
  }

 protected:
  MCTSActorParams params_;
  std::unique_ptr<AI> ai_;
  std::ostream* oo_ = nullptr;
  std::mt19937 rng_;

  BoardFeature get_extractor(const StateForChouFleur& s) {
    // RandomShuffle: static
    // All extractor will go through a
    // random symmetry
    /*if (params_.rotation_flip)    FIXME: we don't have to do rotations in the general setting, right ? PROBABLY OK
      return BoardFeature::RandomShuffle(s, &rng_);
    else*/
      return BoardFeature(s);
  }

  PreEvalResult pre_evaluate(const StateForChouFleur& s, NodeResponse* resp) {
    resp->q_flip = s.nextPlayer() == 2; // S_WHITE;  FIXME: S_WHITE =2, right ? PROBABLY OK

    if (s.terminated()) {
      if (oo_ != nullptr) {
        *oo_ << "Terminal state at " << s.getPly() << " Use TT evaluator"
             << std::endl;
/*        *oo_ << "Moves[" << s.getAllMoves().size()
             << "]: " << s.getAllMovesString() << std::endl;*/
        *oo_ << s.showBoard() << std::endl;
      }
      float final_value = s.evaluate();
      if (oo_ != nullptr)
        *oo_ << "Terminal state. Get raw score." << final_value
             << std::endl;
      resp->value = final_value > 0 ? 1.0 : -1.0;
      // No further action.
      resp->pi.clear();
      return EVAL_DONE;
    } else {
      return EVAL_NEED_NN;
    }
  }

  void post_nn_result(const ChouFleurReply& reply, NodeResponse* resp) {
    if (params_.required_version >= 0 &&
        reply.version != params_.required_version) {
      const std::string msg = "model version " + std::to_string(reply.version) +
          " and required version " + std::to_string(params_.required_version) +
          " are not consistent";
      std::cout << msg << std::endl;
      throw std::runtime_error(msg);
    }

    if (oo_ != nullptr)
      *oo_ << "Got information from neural network" << std::endl;
    resp->value = reply.value;

    const StateForChouFleur& s = reply.bf.state();

    bool pass_enabled = s.getPly() >= params_.ply_pass_enabled;  // FIXME this should always be False, for a game without pass ?
    /*if (params_.remove_pass_if_dangerous) {
      remove_pass_if_dangerous(s, &pass_enabled);
    }*/
    pi2response(reply.bf, reply.pi, pass_enabled, &resp->pi, oo_);
  }

 /* void remove_pass_if_dangerous(const StateForChouFleur& s, bool* pass_enabled) {
    // [TODO] Hack here. The bot never pass first, if the pass leads to an
    // immediate loss..
    if (*pass_enabled && s.lastMove() != M_PASS) {
      bool black_win = s.evaluate(params_.komi) > 0;
      if ((black_win && s.nextPlayer() == S_WHITE) ||
          (!black_win && s.nextPlayer() == S_BLACK)) {
        *pass_enabled = false;
      }
    }
  }*/

  static void normalize(std::vector<std::pair<Coord, float>>* output_pi) {
    assert(output_pi != nullptr);
    float total_prob = 1e-10;
    for (const auto& p : *output_pi) {
      total_prob += p.second;
    }

    for (auto& p : *output_pi) {
      p.second /= total_prob;
    }
  }

  static void pi2response(
      const BoardFeature& bf,
      const std::vector<float>& pi,
      bool pass_enabled,
      std::vector<std::pair<Coord, float>>* output_pi,
      std::ostream* oo = nullptr) {
    const StateForChouFleur& s = bf.state();
    UNUSED(pass_enabled);
    if (oo != nullptr) {
      *oo << "In get_last_pi, #move returned " << pi.size() << std::endl;
      *oo << s.showBoard() << std::endl << std::endl;
    }

    output_pi->clear();

    // No action for terminated state.
    if (s.terminated()) {
      if (oo != nullptr)
        *oo << "Terminal state at " << s.getPly() << std::endl;
      return;
    }

    for (size_t i = 0; i < pi.size(); ++i) {
      // Inv random transform will be applied
      Coord m = bf.action2Coord(i);
      if (oo != nullptr)
        *oo << "  Action " << i << " to Coord "
            << elf::ai::tree_search::ActionTrait<Coord>::to_string(m)
            << std::endl;
      output_pi->push_back(std::make_pair(m, pi[i]));
    }
    // sorting..
    using data_type = std::pair<Coord, float>;

    if (oo != nullptr)
      *oo << "Before sorting" << std::endl;
    std::sort(
        output_pi->begin(),
        output_pi->end(),
        [](const data_type& d1, const data_type& d2) {
          return d1.second > d2.second;
        });
    if (oo != nullptr)
      *oo << "After sorting" << std::endl;

    std::vector<data_type> tmp;
    int i = 0;
    while (true) {
      if (i >= (int)output_pi->size())
        break;
      const data_type& v = output_pi->at(i);
      // Check whether this move is right.
      bool valid = /*(v.first == M_PASS && pass_enabled) ||
          (v.first != M_PASS && */ s.checkMove(v.first);
      if (valid) {
        tmp.push_back(v);
      }

      if (oo != nullptr) {
        *oo << "Predict [" << i << "][" << /*coord2str(v.first) << "]["
            << coord2str2(v.first) << "][" <<*/ v.first << "] " << v.second;
        if (valid)
          *oo << " added" << std::endl;
        else
          *oo << " invalid" << std::endl;
      }
      i++;
    }
/*    if (tmp.empty() && !pass_enabled) {
      // Add pass if there is no valid move.
      tmp.push_back(std::make_pair(M_PASS, 1.0));
    }*/
    *output_pi = tmp;
    normalize(output_pi);
    if (oo != nullptr)
      *oo << "#Valid move: " << output_pi->size() << std::endl;
  }
};

namespace elf {
namespace ai {
namespace tree_search {

template <>
struct ActorTrait<MCTSActor> {
 public:
  static std::string to_string(const MCTSActor& a) {
    return a.info();
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf

class MCTSChouFleurAI : public elf::ai::tree_search::MCTSAI_T<MCTSActor> {
 public:
  MCTSChouFleurAI(
      const elf::ai::tree_search::TSOptions& options,
      std::function<MCTSActor*(int)> gen)
      : elf::ai::tree_search::MCTSAI_T<MCTSActor>(options, gen) {}

  float getValue() const {
    // Check if we need to resign.
    const auto& result = getLastResult();
    if (result.total_visits == 0)
      return result.root_value;
    else
      return result.best_edge_info.getQSA();
  }

  elf::ai::tree_search::MCTSPolicy<Coord> getMCTSPolicy() const {
    const auto& result = getLastResult();
    auto policy = result.mcts_policy;
    // cout << policy.info() << endl;
    policy.normalize();
    return policy;
  }

  void setRequiredVersion(int64_t ver) {
    auto* engine = getEngine();
    assert(engine != nullptr);
    for (size_t i = 0; i < engine->getNumActors(); ++i) {
      engine->getActor(i).setRequiredVersion(ver);
    }
  }
};
