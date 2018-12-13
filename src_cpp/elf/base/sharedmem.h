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
#include <functional>

#include "sharedmem_data.h"
#include "elf/comm/comm.h"
#include "elf/concurrency/ConcurrentQueue.h"

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

inline void state2mem(const Message& msg, SharedMemData& mem) {
  // std::cout << "State2Mem: base_idx: " << msg.base_idx << ", msg.data.size(): " << msg.data.size() 
  //          << ", msg addr: " << std::hex << &msg << std::dec << mem.info() << std::endl;
  int idx = msg.base_idx;
  for (const auto* datum : msg.data) {
    assert(datum != nullptr);
    datum->state_to_mem_funcs.transfer(idx, mem);
    idx++;
  }
}

inline void mem2state(const SharedMemData& mem, Message& msg) {
  // std::cout << "Mem2State: base_idx: " << msg.base_idx << ", msg.data.size(): " << msg.data.size() 
  //           << ", msg addr: " << std::hex << &msg << std::dec << std::endl;
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
      const SharedMemOptions& opts,
      const std::unordered_map<std::string, AnyP>& mem)
      : smem_(opts, mem) {}

  virtual void start() = 0;
  virtual void waitBatchFillMem() = 0;
  virtual void waitReplyReleaseBatch(comm::ReplyStatus batch_status) = 0;

  SharedMemData& data() {
    return smem_;
  }

  virtual ~SharedMem() = default;

 protected:
  SharedMemData smem_;
};

class SharedMemLocal : public SharedMem {
 public:
  SharedMemLocal(
      Server* server,
      const SharedMemOptions& smem_opts,
      const std::unordered_map<std::string, AnyP>& mem)
      : SharedMem(smem_opts, mem), server_(server) {}

  const SharedMemOptions& options() const {
    return smem_.getSharedMemOptionsC();
  }

  void start() override {
    server_->RegServer(options().getRecvOptions().label);
  }

  void waitBatchFillMem() override {
    const auto& opt = options();
    server_->waitBatch(opt.getRecvOptions(), &msgs_from_client_);
    size_t batchsize = 0;
    for (const Message& m : msgs_from_client_) {
      batchsize += m.data.size();
    }
    smem_.setEffectiveBatchSize(batchsize);

    if ((int)batchsize > opt.getBatchSize() ||
        (int)batchsize < opt.getMinBatchSize()) {
      std::cout << "Error: active_batch_size =  " << batchsize
                << ", max_batch_size: " << opt.getBatchSize()
                << ", min_batch_size: " << opt.getMinBatchSize()
                << ", #msg count: " << msgs_from_client_.size() << std::endl;
      assert(false);
    }

    // LOG(INFO) << "Receiver: Batch received. #batch = "
    //           << active_batch_size_ << std::endl;

    if (opt.getTransferType() == SharedMemOptions::SERVER) {
      local_state2mem();
    } else {
      client_state2mem();
    }
  }

  void waitReplyReleaseBatch(comm::ReplyStatus batch_status) override {
    if (options().getTransferType() == SharedMemOptions::SERVER) {
      local_mem2state();
    } else {
      client_mem2state();
    }

    // LOG(INFO) << "Receiver: About to release batch: #batch = "
    //           << active_batch_size_ << std::endl;
    assert(server_ != nullptr);
    server_->ReleaseBatch(msgs_from_client_, batch_status);
    msgs_from_client_.clear();
  }

 private:
  Server* server_ = nullptr;

  // We get a batch of messages from client
  // Note that msgs_from_client_.size() is no longer the batchsize, since one
  // Message could contain multiple states.
  std::vector<Message> msgs_from_client_;

  void local_state2mem() {
    // Send the state to shared memory.
    for (const Message& m : msgs_from_client_) {
      state2mem(m, smem_);
    }
  }

  void client_state2mem() {
    // Send the state to shared memory.
    std::vector<typename Comm::ReplyFunction> msgs;
    for (const Message& m : msgs_from_client_) {
      // LOG(INFO) << "state2mem: Batch " << i << " ptr: " << std::hex
      //           << msgs_from_client_[i].m << std::dec << ", msg address: "
      //           << std::hex << &msgs_from_client_[i] << dec << std::endl;
      msgs.push_back([&]() {
        state2mem(m, smem_);
        // Done one job.
        return comm::DONE_ONE_JOB;
      });
    }
    assert(server_ != nullptr);
    server_->sendClosuresWaitDone(msgs_from_client_, msgs);
  }

  void local_mem2state() {
    // Send the state to shared memory.
    for (Message& m : msgs_from_client_) {
      mem2state(smem_, m);
    }
  }

  void client_mem2state() {
    // Send the state to shared memory.
    std::vector<typename Comm::ReplyFunction> msgs;
    for (Message& m : msgs_from_client_) {
      // LOG(INFO) << "mem2state: Batch " << i << " ptr: " << std::hex
      //           << msgs_from_client_[i].m << dec << std::endl;
      msgs.push_back([&]() {
        mem2state(smem_, m);
        // Done one job.
        return comm::DONE_ONE_JOB;
      });
    }
    assert(server_ != nullptr);
    server_->sendClosuresWaitDone(msgs_from_client_, msgs);
  }
};

template <bool use_const>
void FuncsWithStateT<use_const>::transfer(int msg_idx, SharedMemData_t smem)
    const {
  for (const auto& p : funcs_) {
    auto* anyp = smem[p.first];
    assert(anyp != nullptr);
    p.second(*anyp, msg_idx);
  }
}

using BatchComm = comm::CommT<
    SharedMemData*,
    false,
    concurrency::ConcurrentQueue,
    concurrency::ConcurrentQueue>;
using BatchClient = typename BatchComm::Client;
using BatchServer = typename BatchComm::Server;
using BatchMessage = typename BatchComm::Message;

} // namespace elf

