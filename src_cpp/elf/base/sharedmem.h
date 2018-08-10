/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sstream>
#include <string>
#include <unordered_map>

#include "elf/comm/comm.h"
#include "elf/concurrency/ConcurrentQueue.h"
#include "elf/logging/IndexedLoggerFactory.h"

#include "extractor.h"

namespace elf {

using Comm = typename comm::CommT<
    FuncsWithState*,
    true,
    concurrency::ConcurrentQueue,
    concurrency::ConcurrentQueue>;
// Message sent from client to server
using Message = typename Comm::Message;
using Server = typename Comm::Server;
using Client = typename Comm::Client;

class SharedMemOptions {
 public:
  enum TransferType { SERVER = 0, CLIENT };

  SharedMemOptions(const std::string& label, int batchsize)
      : options_(label, batchsize, 0, 1) {}

  void setIdx(int idx) {
    idx_ = idx;
  }

  void setTimeout(int timeout_usec) {
    options_.wait_opt.timeout_usec = timeout_usec;
  }

  void setMinBatchSize(int minbatchsize) {
    options_.wait_opt.min_batchsize = minbatchsize;
  }

  void setTransferType(TransferType type) {
    type_ = type;
  }

  int getIdx() const {
    return idx_;
  }

  const comm::RecvOptions& getRecvOptions() const {
    return options_;
  }

  comm::WaitOptions& getWaitOptions() {
    return options_.wait_opt;
  }

  const std::string& getLabel() const {
    return options_.label;
  }

  int getBatchSize() const {
    return options_.wait_opt.batchsize;
  }

  int getMinBatchSize() const {
    return options_.wait_opt.min_batchsize;
  }

  TransferType getTransferType() const {
    return type_;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "SMem[" << options_.label << "], idx: " << idx_
       << ", batchsize: " << options_.wait_opt.batchsize;

    if (options_.wait_opt.timeout_usec > 0) {
      ss << ", timeout_usec: " << options_.wait_opt.timeout_usec;
    }

    if (type_ != SERVER) {
      ss << ", transfer_type: " << type_;
    }

    return ss.str();
  }

 private:
  int idx_ = -1;
  comm::RecvOptions options_;
  TransferType type_ = CLIENT;
};

class SharedMem;

inline void state2mem(const Message& msg, SharedMem& mem) {
  // LOG(INFO) << "BatchIdx: " << msg_idx << ", msg addr: "
  //           << std::hex << &msg << std::dec << std::endl;
  int idx = msg.base_idx;
  for (const auto* datum : msg.data) {
    assert(datum != nullptr);
    datum->state_to_mem_funcs.transfer(idx, mem);
    idx++;
  }
}

inline void mem2state(const SharedMem& mem, Message& msg) {
  int idx = msg.base_idx;
  for (const auto* datum : msg.data) {
    assert(datum != nullptr);
    datum->mem_to_state_funcs.transfer(idx, mem);
    idx++;
  }
}

class SharedMem {
 public:
  SharedMem(
      int idx,
      const SharedMemOptions& smem_opts,
      const std::unordered_map<std::string, AnyP>& mem)
      : opts_(smem_opts),
        mem_(mem),
        logger_(elf::logging::getLogger("elf::base::SharedMem-", "")) {
    opts_.setIdx(idx);
  }

  void waitBatchFillMem(Server* server) {
    server->waitBatch(opts_.getRecvOptions(), &msgs_from_client_);
    active_batch_size_ = 0;
    for (const Message& m : msgs_from_client_) {
      active_batch_size_ += m.data.size();
    }

    if ((int)active_batch_size_ > opts_.getBatchSize() ||
        (int)active_batch_size_ < opts_.getMinBatchSize()) {
      logger_->info(
          "Error: active_batch_size = {}, max_batch_size: {}, "
          "min_batch_size: {}, #msg count: {}",
          active_batch_size_,
          opts_.getBatchSize(),
          opts_.getMinBatchSize(),
          msgs_from_client_.size());
      assert(false);
    }

    // LOG(INFO) << "Receiver: Batch received. #batch = "
    //           << active_batch_size_ << std::endl;

    if (opts_.getTransferType() == SharedMemOptions::SERVER) {
      local_state2mem();
    } else {
      client_state2mem(server);
    }
  }

  void waitReplyReleaseBatch(Server* server, comm::ReplyStatus batch_status) {
    if (opts_.getTransferType() == SharedMemOptions::SERVER) {
      local_mem2state();
    } else {
      client_mem2state(server);
    }

    // LOG(INFO) << "Receiver: About to release batch: #batch = "
    //           << active_batch_size_ << std::endl;
    server->ReleaseBatch(msgs_from_client_, batch_status);
    msgs_from_client_.clear();
  }

  const SharedMemOptions& getSharedMemOptions() const {
    return opts_;
  }

  size_t getEffectiveBatchSize() const {
    return active_batch_size_;
  }

  void setTimeout(int timeout_usec) {
    opts_.setTimeout(timeout_usec);
  }

  void setMinBatchSize(int minbatchsize) {
    opts_.setMinBatchSize(minbatchsize);
  }

  std::string info() const {
    std::stringstream ss;
    ss << opts_.info() << std::endl;
    for (const auto& p : mem_) {
      ss << "[" << p.first << "]: " << p.second.info() << std::endl;
    }
    return ss.str();
  }

  AnyP* operator[](const std::string& key) {
    auto it = mem_.find(key);

    if (it != mem_.end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }

  // [TODO] For python to use.
  AnyP* get(const std::string& key) {
    return (*this)[key];
  }

  const AnyP* operator[](const std::string& key) const {
    auto it = mem_.find(key);
    if (it != mem_.end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }

 private:
  SharedMemOptions opts_;
  std::unordered_map<std::string, AnyP> mem_;

  // We get a batch of messages from client
  // Note that msgs_from_client_.size() is no longer the batchsize, since one
  // Message could contain multiple states.
  std::vector<Message> msgs_from_client_;
  size_t active_batch_size_ = 0;

  std::shared_ptr<spdlog::logger> logger_;

  void local_state2mem() {
    // Send the state to shared memory.
    for (const Message& m : msgs_from_client_) {
      state2mem(m, *this);
    }
  }

  void client_state2mem(Server* server) {
    // Send the state to shared memory.
    std::vector<typename Comm::ReplyFunction> msgs;
    for (const Message& m : msgs_from_client_) {
      // LOG(INFO) << "state2mem: Batch " << i << " ptr: " << std::hex
      //           << msgs_from_client_[i].m << std::dec << ", msg address: "
      //           << std::hex << &msgs_from_client_[i] << dec << std::endl;
      msgs.push_back([&]() {
        state2mem(m, *this);
        // Done one job.
        return comm::DONE_ONE_JOB;
      });
    }
    server->sendClosuresWaitDone(msgs_from_client_, msgs);
  }

  void local_mem2state() {
    // Send the state to shared memory.
    for (Message& m : msgs_from_client_) {
      mem2state(*this, m);
    }
  }

  void client_mem2state(Server* server) {
    // Send the state to shared memory.
    std::vector<typename Comm::ReplyFunction> msgs;
    for (Message& m : msgs_from_client_) {
      // LOG(INFO) << "mem2state: Batch " << i << " ptr: " << std::hex
      //           << msgs_from_client_[i].m << dec << std::endl;
      msgs.push_back([&]() {
        mem2state(*this, m);
        // Done one job.
        return comm::DONE_ONE_JOB;
      });
    }
    server->sendClosuresWaitDone(msgs_from_client_, msgs);
  }
};

template <bool use_const>
void FuncsWithStateT<use_const>::transfer(int msg_idx, SharedMem_t smem) const {
  for (const auto& p : funcs_) {
    auto* anyp = smem[p.first];
    assert(anyp != nullptr);
    p.second(*anyp, msg_idx);
  }
}

using BatchComm = comm::CommT<
    SharedMem*,
    false,
    concurrency::ConcurrentQueue,
    concurrency::ConcurrentQueue>;
using BatchClient = typename BatchComm::Client;
using BatchServer = typename BatchComm::Server;
using BatchMessage = typename BatchComm::Message;

} // namespace elf
