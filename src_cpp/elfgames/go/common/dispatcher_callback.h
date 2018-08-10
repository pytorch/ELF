#pragma once

#include "elf/base/context.h"
#include "elf/base/dispatcher.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "record.h"

using Ctrl = elf::Ctrl;
using Addr = elf::Addr;
using ThreadedDispatcher = elf::ThreadedDispatcherT<MsgRequest, RestartReply>;

class DispatcherCallback {
 public:
  DispatcherCallback(ThreadedDispatcher* dispatcher, elf::GameClient* client)
      : client_(client),
        logger_(elf::logging::getLogger(
            "elfgames::go::common::DispatcherCallback-",
            "")) {
    using std::placeholders::_1;
    using std::placeholders::_2;

    dispatcher->Start(
        std::bind(&DispatcherCallback::OnReply, this, _1, _2),
        std::bind(&DispatcherCallback::OnFirstSend, this, _1, _2));
  }

  void OnFirstSend(const Addr& addr, MsgRequest* request) {
    const size_t thread_idx = stoi(addr.label.substr(5));
    if (thread_idx == 0) {
      // Actionable request
      logger_->info(
          "{}, EvalCtrl received new request: {}",
          elf_utils::now(),
          request->info());
    }

    int thread_used = request->client_ctrl.num_game_thread_used;
    if (thread_used < 0)
      return;

    if (thread_idx >= (size_t)thread_used) {
      request->vers.set_wait();
    }
  }

  std::vector<bool> OnReply(
      const std::vector<MsgRequest>& requests,
      std::vector<RestartReply>* p_replies) {
    auto& replies = *p_replies;

    const MsgRequest* request = nullptr;
    size_t n = 0;

    for (size_t i = 0; i < replies.size(); ++i) {
      switch (replies[i]) {
        case RestartReply::UPDATE_MODEL:
        case RestartReply::UPDATE_MODEL_ASYNC:
          if (request != nullptr && *request != requests[i]) {
            logger_->error(
                "{} Request inconsistent. Existing request: {}, current "
                "request: {}",
                elf_utils::now(),
                request->info(),
                requests[i].info());
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
      logger_->info(
          "{} Received actionable request: black_ver = {}, white_ver = {}, "
          "#addrs_to_reply: {}",
          elf_utils::now(),
          request->vers.black_ver,
          request->vers.white_ver,
          n);
      elf::FuncsWithState funcs =
          client_->BindStateToFunctions({start_target_}, &request->vers);
      client_->sendWait({start_target_}, &funcs);

      for (size_t i = 0; i < replies.size(); ++i) {
        RestartReply& r = replies[i];
        if (r == RestartReply::UPDATE_MODEL) {
          r = RestartReply::UPDATE_COMPLETE;
          next_session[i] = true;
        }
      }
    }

    return next_session;
  }

 private:
  elf::GameClient* client_ = nullptr;
  const std::string start_target_ = "game_start";
  std::shared_ptr<spdlog::logger> logger_;
};
