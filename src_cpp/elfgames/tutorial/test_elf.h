#pragma once

#include <string>
#include <thread>
#include <vector>

#include "elf/base/game_context.h"

using namespace std;

using AnyP = elf::AnyP;
using GameContext = elf::GameContext;
using Client = elf::GameClient;
using Extractor = elf::Extractor;
using Options = elf::Options;

struct State {
  int id;
  int value;
  int seq;
  int reply;

  State() {}
  State(const State&) = delete;

  void dumpState(int *state) const {
    // Dump the last n state.
    // cout << "Dump state for id=" << id << ", seq=" << seq << endl;
    *state = value;
  }

  void loadReply(const int* a) {
    // cout << "[" << hex << this << dec << "] load reply for id=" << id << ",
    // seq=" << seq << ", a=" << *a << endl;
    reply = *a;
  }
};

class Game {
 public:
  Game(int idx,
      const std::string& batch_target,
      Client* client)
      : idx_(idx),
        batch_target_(batch_target),
        client_(client) {
          s_.seq = 0;
      }

  void OnAct(elf::game::Base* /*base*/) {
    // client_->oo() << "Starting sending thread " << idx_;
    // mt19937 rng(idx_);
    s_.id = idx_;
    s_.value = idx_ + s_.seq;

    // client_->oo() << "client " << idx_ << " sends #" << j << "...";

    elf::FuncsWithState funcs =
      client_->BindStateToFunctions({batch_target_}, &s_);

    if (client_->sendWait({batch_target_}, &funcs) == comm::SUCCESS) {
      if (s_.reply != 2 * (idx_ + s_.seq) + 1) {
        std::cout << "Error: [" << hex << &s_ << dec << "] client "
          << idx_ << " return from #" << s_.seq
          << ", value: " << s_.value
          << ", reply = " << s_.reply;
      }
    } else {
      // std::cout << "client " << idx_ << " return from #" << j << "
      // failed.";
    }
    s_.seq ++;
  }

 private:
  int idx_;
  std::string batch_target_;
  Client* client_;

  State s_;
};

class MyContext {
 public:
   MyContext(const std::string &batch_name)
     : batch_name_(batch_name) {
   }

   void setGameContext(elf::GCInterface* ctx) {
    Extractor& e = ctx->getExtractor();

    int batchsize = ctx->options().batchsize;
    int num_games = ctx->options().num_game_thread;

    e.addField<int>("value")
        .addExtents(batchsize, {batchsize, 1})
        .addFunction<State>(&State::dumpState);
    e.addField<int>("reply")
        .addExtents(batchsize, {batchsize})
        .addFunction<State>(&State::loadReply);

    using std::placeholders::_1;

    for (int i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new Game(i, batch_name_, ctx->getClient()));
        g->setCallbacks(
            std::bind(&Game::OnAct, games_[i].get(), _1));
      }
    }
  }

 private:
  std::string batch_name_;
  std::vector<std::unique_ptr<Game>> games_;
};
