#pragma once

#include <chrono>
#include "elf/base/game_context.h"
#include "elf/distributed/addrs.h"
#include "elf/distributed/options.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "dispatch_callback.h"

using ThreadedWriter = elf::msg::Client;

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
    ctrl_.sendMail("dispatcher", smsg);
    return -1;
  }

  std::string OnSend() {
    // Send dummy content.
    std::string content = "";
    return content;
  }

 private:
  Ctrl& ctrl_;
};

class Client {
 public:
  Client(const elf::Options& options)
      : options_(options) {}

  void setGameContext(elf::GCInterface* ctx) {
    goFeature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());

    uint64_t num_games = ctx->options().num_game_thread;

    if (ctx->getClient() != nullptr) {
      dispatcher_.reset(new ThreadedDispatcher(ctx->getCtrl(), num_games));
    }

    auto netOptions =
      elf::msg::getNetOptions(options_.common.base, options_.common.net);
    // if no message, sleep every 10s
    netOptions.usec_sleep_when_no_msg = 10000000;
    // Resend after 900s
    netOptions.usec_resend_when_no_msg = 900000000;
    writer_.reset(new ThreadedWriter(netOptions));
    writer_callback_.reset(
        new WriterCallback(writer_.get(), ctx->getCtrl()));

    using std::placeholders::_1;

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new ClientGame(i, options_, dispatcher_.get()));
        g->setCallbacks(
            std::bind(&ClientGame::OnAct, games_[i].get(), _1),
            std::bind(&ClientGame::OnEnd, games_[i].get(), _1),
            [&, i](elf::game::Base*) { dispatcher_->RegGame(i); });
      }
    }

    if (ctx->getClient() != nullptr) {
      dispatcher_callback_.reset(
          new DispatcherCallback(dispatcher_.get(), ctx->getClient()));
    }
  }

  ~Client() {
    dispatcher_.reset(nullptr);
    writer_.reset(nullptr);
    writer_callback_.reset(nullptr);
    dispatcher_callback_.reset(nullptr);
  }

 private:
  std::vector<std::unique_ptr<Game>> games_;
  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<DispatcherCallback> dispatcher_callback_;

  /// ZMQClient
  std::unique_ptr<ThreadedWriter> writer_;
  std::unique_ptr<WriterCallback> writer_callback_;

  const elf::Options options_;

  FeatureExtractor goFeature_;
};
