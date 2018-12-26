#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "elf/base/game_context.h"
#include "elf/distributed/data_loader.h"
#include "elf/distributed/addrs.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include "game_interface.h"

#include "record.h"
#include "options.h"

#include <thread>

namespace elf {

namespace cs {

class DataHolder : public elf::msg::DataInterface {
 public:
  DataHolder(const TrainCtrlOptions &options,
             const ClientManagerOptions &cm_options,
             ServerInterface *server_interface)
    : rng_(time(NULL)) {
    // Register sender for python thread.
    elf::shared::RQCtrl rq_ctrl;
    rq_ctrl.num_reader = options.num_reader;
    rq_ctrl.ctrl.queue_min_size = options.q_min_size;
    rq_ctrl.ctrl.queue_max_size = options.q_max_size;

    replay_buffer_.reset(new ReplayBuffer(rq_ctrl));
    // logger_->info(
    //    "Finished initializing replay_buffer {}", replay_buffer_->info());

    client_mgr_.reset(new ClientManager(cm_options));
    server_interface_ = server_interface;
    assert(server_interface_ != nullptr);
  }

  void OnStart() override {
    server_interface_->onStart();
  }

  elf::shared::InsertInfo OnReceive(
      const std::string& identity,
      const std::string& msg) override {
    (void)identity;
    Records rs = Records::createFromJsonString(msg);
    std::cout << "TrainCtrl: RecvMsg[" << identity << "]: " << rs.size() << std::endl;
    const ClientInfo& info = client_mgr_->updateStates(rs.identity, rs.states);
    return server_interface_->onReceive(std::move(rs), info);
  }

  bool OnReply(const std::string& identity, std::string* msg) override {
    ClientInfo& info = client_mgr_->getClient(identity);

    if (info.justAllocated()) {
      //std::cout << "New allocated: " << identity << ", " << client_mgr_->info()
      //          << std::endl;
    }

    MsgRequest request;
    server_interface_->fillInRequest(info, &request);
    request.client_ctrl.seq = info.seq();
    *msg = request.dumpJsonString();
    info.incSeq();
    std::cout << "TrainCtrl: ReplyMsg[" << identity << "]: " << *msg << std::endl;
    return true;
  }

  ReplayBuffer* getReplayBuffer() {
    return replay_buffer_.get();
  }

  ClientManager *getClientManager() {
    return client_mgr_.get();
  }

 private:
  std::unique_ptr<ReplayBuffer> replay_buffer_;
  std::unique_ptr<ClientManager> client_mgr_;
  std::mt19937 rng_;

  ServerInterface *server_interface_ = nullptr;
};


class Server {
 public:
  Server(const Options& options)
      : options_(options) {}

  void setInterface(ServerInterface *server_interface) {
    server_interface_ = server_interface;
  }

  void setGameContext(elf::GCInterface* ctx) {
    ctx_ = ctx;
    size_t num_games = ctx->options().num_game_thread;

    dataHolder_.reset(new DataHolder(options_.tc_opt, options_.cm_opt, server_interface_));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new _Game(i, this));
        g->setCallbacks(std::bind(&_Game::OnAct, games_[i].get(), _1));
      }
    }

    auto netOptions =
      elf::msg::getNetOptions(options_.base, options_.net);
    // 10s
    netOptions.usec_sleep_when_no_msg = 10000000;
    onlineLoader_.reset(new elf::msg::DataOnlineLoader(netOptions));
    onlineLoader_->start(dataHolder_.get());
  }

  GCInterface *ctx() { return ctx_; }

  DataHolder *getDataHolder() {
    return dataHolder_.get();
  }

  ReplayBuffer *getReplayBuffer() {
    return dataHolder_->getReplayBuffer();
  }

  ClientManager *getClientManager() {
    return dataHolder_->getClientManager();
  }

  ~Server() {
    dataHolder_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

 private:
  struct _Game {
    int game_idx_;
    Server *s_ = nullptr;
    ServerGame *game_ = nullptr;

    _Game(int game_idx, Server *s) : game_idx_(game_idx), s_(s) {
      game_ = s_->server_interface_->createGame(game_idx);
    }

    void OnAct(game::Base* base) {
      auto *reader = s_->getReplayBuffer();
      game_->step(base, reader);
    }
  };

  elf::GCInterface *ctx_ = nullptr;

  std::vector<std::unique_ptr<_Game>> games_;
  std::unique_ptr<DataHolder> dataHolder_;
  std::unique_ptr<elf::msg::DataOnlineLoader> onlineLoader_;

  const Options options_;
  ServerInterface *server_interface_ = nullptr;
};

}  // namespace cs

}  // namespace elf
