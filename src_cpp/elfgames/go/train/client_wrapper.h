#pragma once

#include <chrono>
#include "../common/game_selfplay.h"
#include "../common/record.h"
#include "elf/base/game_context.h"
#include "elf/distributed/addrs.h"
#include "elf/distributed/options.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "../mcts/mcts.h"
#include "elf/ai/tree_search/mcts.h"
#include "../common/game_selfplay.h"

#include "elf/distri/client.h"

using elf::cs::ClientGame;
using elf::cs::ClientInterface;
using elf::Addr;
using elf::cs::Client;
using elf::cs::MsgRequest;

class ClientWrapper : public ClientInterface {
 public:
  ClientWrapper(const GameOptionsSelfPlay& options)
      : options_(options),
        goFeature_(options.common.use_df_feature, 1),
        logger_(elf::logging::getLogger("Client-", "")) {}

  void set(Client *client) {
    assert(client);
    client_obj_ = client; 

    auto *ctx = client_obj_->ctx();
    ctx->getExtractor().merge(goFeature_.registerExtractor(ctx->options().batchsize));
    client_obj_->setInterface(this);
  }

  ClientGame *createGame(int idx) override {
    auto *p = new GoGameSelfPlay(idx, options_, game_stats_);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      games_.emplace_back(p);
    }

    return p;
  }
      
  void onFirstSend(const Addr& addr, MsgRequest* request) override {
    assert(request != nullptr);

    Request req = Request::createFromJson(request->state);

    const size_t thread_idx = stoi(addr.label.substr(5));
    if (thread_idx == 0) {
      // Actionable request
      //std::cout << elf_utils::now()
      //          << ", EvalCtrl get new request: " << request->info()
      //          << std::endl;
    }

    int thread_used = req.num_game_thread_used;
    if (thread_used < 0)
      return;

    if (thread_idx >= (size_t)thread_used) {
      req.vers.set_wait();
      req.setJsonFields(request->state); 
    }
  }

  std::vector<bool> onReply(
      const std::vector<MsgRequest>& requests,
      std::vector<MsgReply>* p_replies) override {
    assert(p_replies != nullptr);
    auto& replies = *p_replies;

    const MsgRequest* request = nullptr;
    auto *client = client_obj_->ctx()->getClient();
    size_t n = 0;

    for (size_t i = 0; i < replies.size(); ++i) {
      // std::cout << "EvalCtrl: Get confirm from " << msg.second.result << ",
      // game_idx = " << msg.second.game_idx << std::endl;
      switch (replies[i]) {
        case UPDATE_MODEL:
        case UPDATE_MODEL_ASYNC:
          if (request != nullptr && *request != requests[i]) {
            std::cout << elf_utils::now()
              << "Request inconsistent. existing request: "
              << request->info()
              << ", now request: " << requests[i].info() << std::endl;
            throw std::runtime_error("Request inconsistent!");
          }
          request = &requests[i];
          n++;
          break;
        default:
          break;
      }
    }

    std::vector<bool> next_session(replies.size(), false);

    if (request != nullptr) {
      Request req = Request::createFromJson(request->state);
      // Once it is done, send to Python side.
      //std::cout << elf_utils::now() << " Get actionable request: black_ver = "
      //          << request->vers.black_ver
      //          << ", white_ver = " << request->vers.white_ver
      //          << ", #addrs_to_reply: " << n << std::endl;
      client->sendWait("game_start", req.vers);

      for (size_t i = 0; i < replies.size(); ++i) {
        auto& r = replies[i];
        if (r == UPDATE_MODEL) {
          r = UPDATE_COMPLETE;
          next_session[i] = true;
        }
      }
    }

    return next_session;
  }

  // customized functions.
  const GoGameSelfPlay* getGame(int idx) const {
    return games_[idx].get();
  }

  std::map<std::string, int> getParams() const {
    return goFeature_.getParams();
  }

  const GameStats& getGameStats() const {
    return game_stats_;
  }

  // Used in client side.
  void setRequest(
      int64_t black_ver,
      int64_t white_ver,
      float thres,
      int numThreads = -1) {

    auto *dispatcher = client_obj_->getThreadedDispatcher();

    if (dispatcher == nullptr) {
      //std::cout << "Dispatcher is nullptr, skipping request!" << std::endl;
      return;
    }

    Request request;
    request.vers.black_ver = black_ver;
    request.vers.white_ver = white_ver;
    request.vers.mcts_opt = options_.common.mcts;
    request.resign_thres = thres;
    request.num_game_thread_used = numThreads;

    MsgRequest msg_request;
    request.setJsonFields(msg_request.state);

    dispatcher->sendToThread(msg_request);
  }

 private:
  const GameOptionsSelfPlay options_;

  // Common statistics.
  GameStats game_stats_;
  Client *client_obj_ = nullptr;

  std::mutex mutex_;
  std::vector<std::unique_ptr<GoGameSelfPlay>> games_;

  GoFeature goFeature_;

  std::shared_ptr<spdlog::logger> logger_;
};

