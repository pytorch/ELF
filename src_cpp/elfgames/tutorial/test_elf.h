#pragma once

#include <string>
#include <thread>
#include <vector>

#include "game.h"
#include "elf/base/game_context.h"

using namespace std;

using AnyP = elf::AnyP;
using GameContext = elf::GameContext;
using Client = elf::GameClient;
using Extractor = elf::Extractor;
using Options = elf::Options;

class Game {
 public:
  Game(int idx,
      const std::string& batch_target,
      Client* client)
      : idx_(idx),
        batch_target_(batch_target),
        client_(client) {
      }

  void OnAct(elf::game::Base* /*base*/) {
    // client_->oo() << "Starting sending thread " << idx_;
    // mt19937 rng(idx_);
    w_.setIdx(idx_);

    elf::FuncsWithState funcs =
      client_->BindStateToFunctions({batch_target_}, &w_);
    bool success = (client_->sendWait({batch_target_}, &funcs) == comm::SUCCESS);
    w_.step(success);
  }

 private:
  int idx_;
  std::string batch_target_;
  Client* client_;

  game::World w_;
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
        .addFunction<game::World>(&game::getStateFeature);
    e.addField<int>("reply")
        .addExtents(batchsize, {batchsize})
        .addFunction<game::World>(&game::setReply);

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
