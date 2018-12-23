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

class TrainCtrl : public elf::msg::DataInterface {
 public:
  TrainCtrl(const TrainCtrlOptions &options, 
            const ClientManagerOptions &cm_options, 
            ServerInterface *server_interface) 
    : rng_(time(NULL)) {
    // Register sender for python thread.
    elf::shared::RQCtrl rq_ctrl;
    rq_ctrl.num_reader = options.num_reader;
    rq_ctrl.ctrl.queue_min_size = options.q_min_size;
    rq_ctrl.ctrl.queue_max_size = options.q_max_size;

    replay_buffer_.reset(new ReplayBuffer(rq_ctrl));
    client_mgr_.reset(new ClientManager(cm_options));
    server_interface_ = server_interface;
    assert(server_interface_ != nullptr);
  }

  void OnStart() override { 
    server_interface_->OnStart();
  }

  elf::shared::InsertInfo OnReceive(
      const std::string& identity,
      const std::string& msg) override {
    (void)identity;
    Records rs = Records::createFromJsonString(msg);
    std::cout << "TrainCtrl: RecvMsg[" << identity << "]: " << rs.size() << std::endl;
    const ClientInfo& info = client_mgr_->updateStates(rs.identity, rs.states);
    return server_interface_->OnReceive(rs, info, replay_buffer_.get());
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

 private:
  // Ctrl& ctrl_;
  std::unique_ptr<ReplayBuffer> replay_buffer_;
  std::unique_ptr<ClientManager> client_mgr_;
  std::mt19937 rng_;

  ServerInterface *server_interface_ = nullptr;
};


class Server {
 public:
  Server(const Options& options)
      : options_(options) {}

  void setFactory(Factory factory) {
    factory_ = factory;
  }

  void setGameContext(elf::GameContext* ctx) {
    size_t num_games = ctx->options().num_game_thread;

    server_interface_ = std::move(factory_.getServerInterface());
    trainCtrl_.reset(new TrainCtrl(options_.tc_opt, options_.cm_opt, server_interface_.get()));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new ServerGame(i, this));
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
    assert(! games_.empty());
    /*
    const GameInterface &game = *games_[0];
    return game.getParams();
    */
    return std::unordered_map<std::string, int>(); 
  }

  ~Server() {
    trainCtrl_.reset(nullptr);
    onlineLoader_.reset(nullptr);
  }

 private:
  struct ServerGame {
    int game_idx_;
    Server *s_ = nullptr;

    ServerGame(int game_idx, Server *s) : game_idx_(game_idx), s_(s) { }

    void OnAct(game::Base* base) {
      size_t n = s_->options_.server_num_state_pushed_per_thread;

      std::vector<std::unique_ptr<GameInterface>> senders(n);
      std::vector<FuncsWithState> funcsToSend;
      auto *client = base->client();
      auto binder = client->getBinder();

      auto *reader = s_->trainCtrl_->getReplayBuffer();

      for (size_t i = 0; i < n; ++i) {
        while (true) {
          // std::cout << "[" << _game_idx << "][" << i << "] Before get sampler "
          // << std::endl;
          int q_idx;
          auto sampler = reader->getSamplerWithParity(&base->rng(), &q_idx);
          const Record* r = sampler.sample();
          if (r == nullptr) {
            continue;
          }

          senders[i] = std::move(s_->factory_.gameFromJson(r->request.state, &base->rng()));
          if (senders[i].get() != nullptr) {
            funcsToSend.push_back(binder.BindStateToFunctions(
                {"train"}, senders[i].get()));
            break;
          }
        }
      }

      std::vector<elf::FuncsWithState*> funcPtrsToSend(funcsToSend.size());
      for (size_t i = 0; i < funcsToSend.size(); ++i) {
        funcPtrsToSend[i] = &funcsToSend[i];
      }

      client->sendBatchWait({"train"}, funcPtrsToSend);
    }
  };

  Ctrl ctrl_;

  std::vector<std::unique_ptr<ServerGame>> games_;
  std::unique_ptr<TrainCtrl> trainCtrl_;
  std::unique_ptr<elf::msg::DataOnlineLoader> onlineLoader_;
  std::unique_ptr<ServerInterface> server_interface_;

  const Options options_;
  Factory factory_;
};

}  // namespace cs

}  // namespace elf
