#pragma once

#include <chrono>
#include "elf/interface/game_interface.h"
#include "elf/base/ctrl.h"
#include "elf/distributed/addrs.h"
#include "elf/distributed/options.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "dispatcher_callback.h"
#include "record.h"
#include "options.h"

#include "game_interface.h"

namespace elf { 

namespace cs {

using ThreadedWriter = msg::Client;

class WriterCallback {
 public:
  WriterCallback(ThreadedWriter* writer, Ctrl& ctrl)
      : ctrl_(ctrl) {
    writer->setCallbacks(
        std::bind(&WriterCallback::OnSend, this),
        std::bind(&WriterCallback::OnRecv, this, std::placeholders::_1));
    writer->start();
  }

  int64_t OnRecv(const std::string& smsg) {
    // Send data.
    std::cout << "WriterCB: RecvMsg: " << smsg << std::endl;
    ctrl_.sendMail("dispatcher", 
        MsgRequest::createFromJson(json::parse(smsg)));
    return -1;
  }

  std::string OnSend() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string msg = records_.dumpJsonString(); 
    std::cout << "WriterCB: SendMsg: " << records_.size() << std::endl;
    records_.clear();
    return msg;
  }

  void addRecord(const GameInterface &s) {
    std::lock_guard<std::mutex> lock(mutex_);
    Record record;
    record.request.state = s.to_json();
    // Not used. 
    // record.result.reply = r;
    records_.addRecord(std::move(record));
  }

 private:
  Ctrl& ctrl_;
  std::mutex mutex_;
  Records records_;
};

class Client {
 public:
  Client(const Options &options) : options_(options) {}

  void setFactory(Factory factory) {
    factory_ = factory;
  }

  void setGameContext(elf::GCInterface* ctx) {
    uint64_t num_games = ctx->options().num_game_thread;

    if (ctx->getClient() != nullptr) {
      dispatcher_.reset(new ThreadedDispatcher(ctrl_, num_games));
    }

    auto netOptions =
      elf::msg::getNetOptions(options_.base, options_.net);
    // if no message, sleep every 10s
    netOptions.usec_sleep_when_no_msg = 10000000;
    // Resend after 900s
    netOptions.usec_resend_when_no_msg = 900000000;
    writer_.reset(new ThreadedWriter(netOptions));
    writer_callback_.reset(new WriterCallback(writer_.get(), ctrl_));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new ClientGame(i, this));
        g->setCallbacks(
            std::bind(&ClientGame::OnAct, games_[i].get(), _1),
            std::bind(&ClientGame::OnEnd, games_[i].get(), _1),
            [&, i](elf::game::Base*) { dispatcher_->RegGame(i); });
      }
    }

    if (ctx->getClient() != nullptr) {
      dispatcher_callback_.reset(new DispatcherCallback(dispatcher_.get()));
    }
  }

  std::unordered_map<std::string, int> getParams() const {
    assert(! games_.empty());
    const GameInterface &game = *games_[0]->state_;
    return game.getParams();
  }

  ~Client() {
    dispatcher_.reset(nullptr);
    writer_.reset(nullptr);
    writer_callback_.reset(nullptr);
    dispatcher_callback_.reset(nullptr);
  }

 private:
  struct ClientGame {
    int game_idx_;
    Client *c_ = nullptr;
    std::unique_ptr<GameInterface> state_;
    int counter_ = 0;

    ClientGame(int game_idx, Client *client) 
      : game_idx_(game_idx), c_(client) {
    }

    bool OnReceive(const MsgRequest& request, MsgReply* reply) {
      (void)reply;
      state_ = std::move(c_->factory_.gameFromJson(request.state, nullptr));
      // No next section.
      return false;
    }

    void OnEnd(elf::game::Base*) { }

    void OnAct(elf::game::Base* base) {
      // elf::GameClient* client = base->ctx().client;
      if (counter_ % 5 == 0) {
        using std::placeholders::_1;
        using std::placeholders::_2;
        auto f = std::bind(&ClientGame::OnReceive, this, _1, _2);
        bool block_if_no_message = false;

        do {
          c_->dispatcher_->checkMessage(block_if_no_message, f);
        } while (false);
      }
      counter_ ++;

      elf::GameClientInterface *client = base->client();

      // Simply use info to construct random samples.
      client->sendWait("actor", *state_);

      c_->writer_callback_->addRecord(*state_);

      state_->step();
    }
  };

  Ctrl ctrl_;

  const Options options_;
  Factory factory_;

  std::vector<std::unique_ptr<ClientGame>> games_;
  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<DispatcherCallback> dispatcher_callback_;

  /// ZMQClient
  std::unique_ptr<ThreadedWriter> writer_;
  std::unique_ptr<WriterCallback> writer_callback_;
};

}  // namespace cs

}  // namespace elf
