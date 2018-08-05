#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "elf/base/game_context.h"
#include "elf/distributed/data_loader.h"
#include "elf/distributed/addrs.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include <thread>

class TrainCtrl : public DataInterface {
 public:
  void OnStart() {}
  elf::shared::InsertInfo OnReceive(
      const std::string& identity,
      const std::string& msg) {
    return elf::shared::InsertInfo();
  }

  bool OnReply(const std::string& identity, std::string* msg) {
    // Send reply message once we have messages.
    *msg = "";
    return true;
  }
};

class Server {
 public:
  Server(const elf::Options& options)
      : options_(options) {}

  void setGameContext(elf::GameContext* ctx) {
    feature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());

    size_t num_games = ctx->options().num_game_thread;
    trainCtrl_.reset(new TrainCtrl());

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new ServerGame(i, options_));
        g->setCallbacks(std::bind(&ServerGame::OnAct, games_[i].get(), _1));
      }
    }

    auto netOptions =
      elf::msg::getNetOptions(options_.common.base, options_.common.net);
    // 10s
    netOptions.usec_sleep_when_no_msg = 10000000;
    netOptions.usec_resend_when_no_msg = -1;
    onlineLoader_.reset(new DataOnlineLoader(netOptions));
    onlineLoader_->start(trainCtrl_.get());
  }

  ~Server() {
    trainCtrl_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

 private:
  std::vector<std::unique_ptr<GameServer>> games_;
  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<DataOnlineLoader> onlineLoader_;

  FeatureExtractor feature_;
};
