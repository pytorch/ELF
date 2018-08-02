#pragma once

#include <chrono>
#include "../common/dispatcher_callback.h"
#include "../common/game_selfplay.h"
#include "../common/notifier.h"
#include "../common/record.h"
#include "elf/base/game_context.h"
#include "elf/distributed/addrs.h"
#include "elf/distributed/options.h"
#include "elf/distributed/shared_rw_buffer3.h"

using ThreadedDispatcher = GoGameSelfPlay::ThreadedDispatcher;
using ThreadedWriter = elf::msg::Client;

class WriterCallback {
 public:
  WriterCallback(
      ThreadedWriter* writer,
      Ctrl& ctrl,
      GameNotifier& game_notifier)
      : ctrl_(ctrl), game_notifier_(game_notifier) {
    writer->setCallbacks(
        std::bind(&WriterCallback::OnSend, this),
        std::bind(&WriterCallback::OnRecv, this, std::placeholders::_1));
    writer->start();
  }

  int64_t OnRecv(const std::string& smsg) {
    json j = json::parse(smsg);
    msg_ = MsgRequestSeq::createFromJson(j);

    ctrl_.sendMail("dispatcher", msg_.request);
    return msg_.seq;
  }

  std::string OnSend() {
    int num_record = 0;
    std::string content = game_notifier_.DumpRecords(&num_record);

    if (msg_.request.vers.wait()) {
      std::this_thread::sleep_for(std::chrono::seconds(30));
    } else {
      if (num_record == 0)
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
    /*
       std::cout << "Sending state update[" << records_.identity << "][" <<
       elf_utils::now() << "]"; for (const auto& s : records_.states) {
       std::cout
       << s.second.info() << ", ";
       }
       std::cout << std::endl;
       */
    return content;
  }

 private:
  MsgRequestSeq msg_;
  Ctrl& ctrl_;
  GameNotifier& game_notifier_;
};

class Client {
 public:
  Client(const GameOptionsSelfPlay& options)
      : options_(options),
        goFeature_(options.common.use_df_feature, 1),
        logger_(elf::logging::getLogger("Client-", "")) {}

  void setGameContext(elf::GCInterface* ctx) {
    goFeature_.registerExtractor(ctx->options().batchsize, ctx->getExtractor());

    uint64_t num_games = ctx->options().num_game_thread;

    if (ctx->getClient() != nullptr) {
      dispatcher_.reset(new ThreadedDispatcher(ctx->getCtrl(), num_games));
    }

    using std::placeholders::_1;

    if (options_.common.mode == "selfplay") {
      auto netOptions =
          elf::msg::getNetOptions(options_.common.base, options_.common.net);
      // if no message, sleep every 10s
      netOptions.usec_sleep_when_no_msg = 10000000;
      // Resend after 900s
      netOptions.usec_resend_when_no_msg = 900000000;
      writer_.reset(new ThreadedWriter(netOptions));
      notifier_.reset(
          new GameNotifier(writer_->identity(), options_, ctx->getClient()));

      writer_callback_.reset(
          new WriterCallback(writer_.get(), ctx->getCtrl(), *notifier_));
    } else if (options_.common.mode == "online") {
    } else {
      throw std::range_error(
          "options.mode not recognized! " + options_.common.mode);
    }

    for (size_t i = 0; i < num_games; ++i) {
      auto* g = ctx->getGame(i);
      if (g != nullptr) {
        games_.emplace_back(new GoGameSelfPlay(
            i, options_, dispatcher_.get(), notifier_.get()));
        g->setCallbacks(
            std::bind(&GoGameSelfPlay::OnAct, games_[i].get(), _1),
            std::bind(&GoGameSelfPlay::OnEnd, games_[i].get(), _1),
            [&, i](elf::game::Base*) { dispatcher_->RegGame(i); });
      }
    }

    if (ctx->getClient() != nullptr) {
      dispatcher_callback_.reset(
          new DispatcherCallback(dispatcher_.get(), ctx->getClient()));
    }
  }

  const GoGameSelfPlay* getGame(int idx) const {
    return games_[idx].get();
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

  ~Client() {
    dispatcher_.reset(nullptr);
    writer_.reset(nullptr);
    writer_callback_.reset(nullptr);
    dispatcher_callback_.reset(nullptr);
  }

  const GameStats& getGameStats() const {
    assert(notifier_ != nullptr);
    return notifier_->getGameStats();
  }

  // Used in client side.
  void setRequest(
      int64_t black_ver,
      int64_t white_ver,
      float thres,
      int numThreads = -1) {
    if (dispatcher_ == nullptr) {
      //std::cout << "Dispatcher is nullptr, skipping request!" << std::endl;
      return;
    }
    MsgRequest request;
    request.vers.black_ver = black_ver;
    request.vers.white_ver = white_ver;
    request.vers.mcts_opt = options_.common.mcts;
    request.client_ctrl.resign_thres = thres;
    request.client_ctrl.num_game_thread_used = numThreads;
    dispatcher_->sendToThread(request);
  }

 private:
  std::vector<std::unique_ptr<GoGameSelfPlay>> games_;
  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<DispatcherCallback> dispatcher_callback_;
  std::unique_ptr<GameNotifier> notifier_;

  /// ZMQClient
  std::unique_ptr<ThreadedWriter> writer_;
  std::unique_ptr<WriterCallback> writer_callback_;

  const GameOptionsSelfPlay options_;

  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;
};
