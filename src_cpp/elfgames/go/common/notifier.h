#pragma once

#include "../mcts/mcts.h"
#include "elf/ai/tree_search/mcts.h"
#include "go_state_ext.h"
#include "record.h"

#include "elf/distri/game_interface.h"
#include "elf/interface/game_interface.h"

using elf::cs::ServerFactory;
using elf::cs::ClientFactory;
using elf::cs::ServerInterface;
using elf::cs::ClientInterface;
using elf::cs::GameInterface;
using elf::Addr;

class Wrapper {
 public:
  Wrapper(elf::GCInterface *ctx) {
    client_ = ctx->getClient();
  }

  ServerFactory getServerFactory() {
    ServerFactory f;
    f.createServerInterface = [](int idx) {
      return std::unique_ptr<ServerInterface>(new ServerWrapper());
    };
    f.createGameInterface = [](int idx) {
      return std::unique_ptr<GameInterface>(new GoStateExt());
    };
    return f;
  } 

  ClientFactory getClientFactory() {
    ClientFactory f;
    f.createClientInterface = [&](int idx) {
      return std::unique_ptr<ClientInterface>(new GoGameSelfPlay(idx, game_stats_));
    };

    using std::placeholders::_1;
    using std::placeholders::_2;

    f.onFirstSend = std::bind(&dispatcherOnFirstSend, this, _1, _2);
    f.onReply = std::bind(&dispatcherOnReply, this, _1, _2);
    return f;
  }
      
  void dispatcherOnFirstSend(const Addr& addr, MsgRequest* request) {
    assert(request != nullptr);

    const size_t thread_idx = stoi(addr.label.substr(5));
    if (thread_idx == 0) {
      // Actionable request
      //std::cout << elf_utils::now()
      //          << ", EvalCtrl get new request: " << request->info()
      //          << std::endl;
    }

    int thread_used = request->client_ctrl.num_game_thread_used;
    if (thread_used < 0)
      return;

    if (thread_idx >= (size_t)thread_used) {
      ModelPairs p = request->state["vers"];
      p.set_wait();
      request->state["vers"] = p;
    }
  }

  std::vector<bool> dispatcherOnReply(
      const std::vector<MsgRequest>& requests,
      std::vector<RestartReply>* p_replies) {
    assert(p_replies != nullptr);
    auto& replies = *p_replies;

    const MsgRequest* request = nullptr;
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
      // Once it is done, send to Python side.
      //std::cout << elf_utils::now() << " Get actionable request: black_ver = "
      //          << request->vers.black_ver
      //          << ", white_ver = " << request->vers.white_ver
      //          << ", #addrs_to_reply: " << n << std::endl;
      client_->sendWait("game_start", &request->vers);

      for (size_t i = 0; i < replies.size(); ++i) {
        RestartReply& r = replies[i];
        if (r == UPDATE_MODEL) {
          r = UPDATE_COMPLETE;
          next_session[i] = true;
        }
      }
    }

    return next_session;
  }

 private:
  // Common statistics.
  GameStats game_stats_;
  elf::GameClientInterface *client_ = nullptr;
};

