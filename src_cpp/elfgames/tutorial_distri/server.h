#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "elf/base/game_context.h"
#include "elf/distributed/data_loader.h"
#include "elf/distributed/addrs.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include "record.h"
#include "options.h"
#include "server_game.h"
#include "feature.h"

#include <thread>

using ReplayBuffer = elf::shared::ReaderQueuesT<Record>;

class TrainCtrl : public elf::msg::DataInterface {
 public:
  // TrainCtrl(Ctrl& ctrl) : ctrl_(ctrl), rng_(time(NULL)) {
  TrainCtrl(Ctrl& ctrl) : rng_(time(NULL)) {
    (void)ctrl;
    // Register sender for python thread.
    elf::shared::RQCtrl rq_ctrl;
    rq_ctrl.num_reader = 20;
    rq_ctrl.ctrl.queue_min_size = 10;
    rq_ctrl.ctrl.queue_max_size = 1000;

    replay_buffer_.reset(new ReplayBuffer(rq_ctrl));
  }

  void OnStart() override {}

  elf::shared::InsertInfo OnReceive(
      const std::string& identity,
      const std::string& msg) override {
    std::cout << "TrainCtrl: RecvMsg[" << identity << "]: " << msg << std::endl;
    (void)identity;
    Records rs = Records::createFromJsonString(msg);

    elf::shared::InsertInfo insert_info;
    for (size_t i = 0; i < rs.records.size(); ++i) {
      const Record& r = rs.records[i];
      insert_info +=
        replay_buffer_->Insert(Record(r), &rng_);
    }

    return insert_info;
  }

  bool OnReply(const std::string& identity, std::string* msg) override {
    (void)identity;

    // Send new request to that client.
    MsgRequest request;
    request.state.content = rng_() % 100;
    *msg = request.dumpJsonString();
    std::cout << "TrainCtrl: ReplyMsg[" << identity << "]: " << *msg << std::endl;
    return true;
  }

  ReplayBuffer* getReplayBuffer() {
    return replay_buffer_.get();
  }

 private:
  // Ctrl& ctrl_;
  std::unique_ptr<ReplayBuffer> replay_buffer_;
  std::mt19937 rng_;
};


class Server {
 public:
  Server(const GameOptions& options)
      : options_(options), feature_(options) {}

  void setGameContext(elf::GameContext* ctx) {
    feature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());

    size_t num_games = ctx->options().num_game_thread;
    trainCtrl_.reset(new TrainCtrl(ctx->getCtrl()));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new ServerGame(i, options_, trainCtrl_->getReplayBuffer()));
        g->setCallbacks(std::bind(&ServerGame::OnAct, games_[i].get(), _1));
      }
    }

    auto netOptions =
      elf::msg::getNetOptions(options_.base, options_.net);
    // 10s
    netOptions.usec_sleep_when_no_msg = 10000000;
    netOptions.usec_resend_when_no_msg = -1;
    onlineLoader_.reset(new elf::msg::DataOnlineLoader(netOptions));
    onlineLoader_->start(trainCtrl_.get());
  }

  std::unordered_map<std::string, int> getParams() const {
    return std::unordered_map<std::string, int>{
      { "input_dim", options_.input_dim },
      { "num_action", options_.num_action },
    };
  }

  ~Server() {
    trainCtrl_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

 private:
  std::vector<std::unique_ptr<ServerGame>> games_;
  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<elf::msg::DataOnlineLoader> onlineLoader_;

  const GameOptions options_;

  Feature feature_;
};
