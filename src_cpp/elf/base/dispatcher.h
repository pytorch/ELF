#pragma once

#include <iostream>
#include "../concurrency/ConcurrentQueue.h"
#include "ctrl.h"

namespace elf {

using ThreadedCtrlBase = ThreadedCtrlBaseT<elf::concurrency::ConcurrentQueue>;
using Ctrl = CtrlT<elf::concurrency::ConcurrentQueue>;

template <typename S, typename R>
class ThreadedDispatcherT : public ThreadedCtrlBase {
 public:
  using ServerFirstSend = std::function<void(const Addr&, S*)>;
  using ServerReply =
      std::function<std::vector<bool>(const std::vector<S>&, std::vector<R>*)>;
  using ThreadRecv = std::function<bool(const S&, R*)>;

  ThreadedDispatcherT(Ctrl& ctrl, int num_games)
      : ThreadedCtrlBase(ctrl, 500), num_games_(num_games) {}

  void Start(ServerReply replier, ServerFirstSend first_send = nullptr) {
    server_replier_ = replier;
    server_first_send_ = first_send;
    start<S, std::pair<Addr, R>>("dispatcher");
  }

  // Called by game threads
  void RegGame(int game_idx) {
    ctrl_.reg("game_" + std::to_string(game_idx));
    ctrl_.addMailbox<S, R>();
    // cout << "Register game " << game_idx << endl;
    game_counter_.increment();
  }

  void checkMessage(bool block_wait, ThreadRecv on_receive) {
    S s;
    if (!block_wait) {
      if (!ctrl_.peekMail(&s, 0)) {
        return;
      }
    } else {
      ctrl_.waitMail(&s);
    }

    R reply;
    while (true) {
      // Once you receive, you need to send a reply.
      bool next_session = on_receive(s, &reply);
      ctrl_.sendMail(addr_, std::make_pair(ctrl_.getAddr(), reply));
      if (!next_session)
        break;
      ctrl_.waitMail(&reply);
    }
  }

 protected:
  elf::concurrency::Counter<int> game_counter_;
  std::vector<Addr> addrs_;
  std::unordered_map<Addr, size_t> addr2idx_;
  const int num_games_;

  bool just_started_ = true;
  S last_msg_;

  ServerReply server_replier_ = nullptr;
  ServerFirstSend server_first_send_ = nullptr;

  void before_loop() override {
    // Wait for all games + this processing thread.
    std::cout << "Wait all games[" << num_games_
              << "] to register their mailbox" << std::endl;
    game_counter_.waitUntilCount(num_games_);
    game_counter_.reset();
    std::cout << "All games [" << num_games_ << "] registered" << std::endl;

    addrs_ = ctrl_.filterPrefix(std::string("game"));
    for (size_t i = 0; i < addrs_.size(); ++i) {
      addr2idx_[addrs_[i]] = i;
    }
  }

  void on_thread() override {
    // cout << "Register Recv threads" << endl;
    S msg;
    if (ctrl_.peekMail(&msg, 0)) {
      if (just_started_ || msg != last_msg_) {
        process_request(msg);
        last_msg_ = msg;
        just_started_ = false;
      }
    }
  }

  void process_request(const S& s) {
    size_t n = addrs_.size();

    std::vector<S> requests(n, s);

    for (size_t i = 0; i < n; ++i) {
      const Addr& addr = addrs_[i];
      if (server_first_send_ != nullptr) {
        server_first_send_(addr, &requests[i]);
      }
      ctrl_.sendMail(addr, requests[i]);
    }

    std::vector<R> replies(n);
    std::vector<bool> status(n, true);
    size_t active_n = n;

    while (true) {
      for (size_t i = 0; i < active_n; ++i) {
        std::pair<Addr, R> data;
        ctrl_.waitMail(&data);

        auto it = addr2idx_.find(data.first);
        assert(it != addr2idx_.end());

        size_t idx = it->second;
        assert(status[idx]);

        replies[idx] = data.second;
      }

      auto next_session = server_replier_(requests, &replies);
      active_n = 0;
      for (size_t i = 0; i < n; ++i) {
        if (!next_session[i])
          status[i] = false;
        if (status[i])
          active_n++;
      }
      if (active_n == 0)
        break;

      for (size_t i = 0; i < n; ++i) {
        if (status[i]) {
          ctrl_.sendMail(addrs_[i], replies[i]);
        }
      }
    }
  }
};

} // namespace elf
