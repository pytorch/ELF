/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

#include "elf/concurrency/Counter.h"

#include "base.h"

namespace comm {

template <
    typename Data,
    typename Info,
    typename Reply,
    template <typename> class ClientQueue,
    template <typename> class ServerQueue>
class NodeT;

template <
    typename Data,
    typename Info,
    typename Reply,
    template <typename> class ClientQueue,
    template <typename> class ServerQueue>
struct MsgT {
  using ClientToServer = NodeT<Data, Info, Reply, ClientQueue, ServerQueue>;
  using ServerToClient = NodeT<Reply, Info, Data, ServerQueue, ClientQueue>;

  ClientToServer* from = nullptr;
  ServerToClient* to = nullptr;
  std::vector<Data> data;
  Info info;
  size_t base_idx = 0;

  MsgT(ClientToServer* from, ServerToClient* to, const std::vector<Data>& in, const Info &info) 
      : from(from), to(to), data(in), info(info) {}

  MsgT(ClientToServer* from, ServerToClient* to, Data in, const Info &info) 
    : from(from), to(to), info(info) {
    data.push_back(in);
  }

  MsgT() {}
};

template <
    typename Data,
    typename Info,
    typename Reply,
    template <typename> class MyQueue,
    template <typename> class PartnerQueue>
class NodeT {
 public:
  using Node = NodeT<Data, Info, Reply, MyQueue, PartnerQueue>;
  // using DualNode = NodeT<R, S, CCQ_R, CCQ_S>;

  using SendMsg = MsgT<Data, Info, Reply, MyQueue, PartnerQueue>;
  using RecvMsg = MsgT<Reply, Info, Data, PartnerQueue, MyQueue>;

  bool startSession(const std::vector<SendMsg>& targets) {
    if (n_ > 0) {
      return false;
    }

    for (const auto& pa : targets) {
      pa.to->EnqueueMessage(SendMsg(this, pa.to, pa.data, pa.info));
    }

    n_ = targets.size();
    return true;
  }

  void waitSessionEnd(int timeout_usec = 0) {
    (void)timeout_usec;
    replyCount_.waitUntilCount(n_);
    n_ = 0;
    replyCount_.reset();
  }

  bool waitSessionInvite(
      const WaitOptions& opt,
      std::vector<RecvMsg>* messages) {
    assert(opt.batchsize > 0);

    messages->clear();

    size_t data_count = 0;

    while (true) {
      RecvMsg message;

      bool use_timeout =
          ((int)data_count >= opt.min_batchsize && opt.timeout_usec > 0);
      if (!get_msg(opt, use_timeout, &message))
        break;

      if ((int)(message.data.size() + data_count) > opt.batchsize) {
        unpop_msg(message);
        break;
      }

      // No empty package is allowed.
      assert(!message.data.empty());

      message.base_idx = data_count;
      messages->push_back(message);
      data_count += message.data.size();

      // LOG(INFO) << "Get a message, #m: "
      //           << data_count << std::endl;
      if ((int)data_count == opt.batchsize) {
        break;
      }
    }
    return true;
  }

  void notifySessionInvite() {
    replyCount_.increment();
  }

  void EnqueueMessage(RecvMsg&& msg) {
    q_.push(msg);
  }

 private:
  int n_ = 0;

  RecvMsg unprocessed_msg_;
  // Concurrent Queue.
  MyQueue<RecvMsg> q_;

  elf::concurrency::Counter<int> replyCount_;

  void unpop_msg(const RecvMsg& msg) {
    assert(unprocessed_msg_.data.empty());
    unprocessed_msg_ = msg;
  }

  bool get_msg(const WaitOptions& opt, bool use_timeout, RecvMsg* msg) {
    if (!unprocessed_msg_.data.empty()) {
      *msg = unprocessed_msg_;
      unprocessed_msg_.data.clear();
      return true;
    }
    if (use_timeout) {
      // use timeout.
      // LOG(INFO) << "Timeout. " << hex << this
      //           << dec
      //           << ", opt.timeout_usec = "
      //           << opt.timeout_usec
      //           << ", messages->size() = "
      //           << messages->size()
      //           << std::endl;
      return q_.pop(msg, std::chrono::microseconds(opt.timeout_usec));
    } else {
      // This will block.
      q_.pop(msg);
      return true;
    }
  }
};

} // namespace comm
