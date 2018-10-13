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

#include "elf/comm/comm.h"
#include "elf/concurrency/ConcurrentQueue.h"

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

  void setLabelIdx(int label_idx) {
    label_idx_ = label_idx;
  }

  void setTimeout(int timeout_usec) {
    options_.wait_opt.timeout_usec = timeout_usec;
  }

  void setMinBatchSize(int minbatchsize) {
    options_.wait_opt.min_batchsize = minbatchsize;
  }

  void setBatchSize(int batchsize) {
    options_.wait_opt.batchsize = batchsize;
  }

  void setTransferType(TransferType type) {
    type_ = type;
  }

  int getIdx() const {
    return idx_;
  }

  int getLabelIdx() const {
    return label_idx_;
  }

  const comm::RecvOptions& getRecvOptions() const {
    return options_;
  }

  comm::RecvOptions& getRecvOptions() {
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
       << ", label_idx: " << label_idx_
       << ", batchsize: " << options_.wait_opt.batchsize;

    if (options_.wait_opt.timeout_usec > 0) {
      ss << ", timeout_usec: " << options_.wait_opt.timeout_usec;
    }

    if (type_ != SERVER) {
      ss << ", transfer_type: " << type_;
    }

    return ss.str();
  }

  friend bool operator==(const SharedMemOptions &op1, const SharedMemOptions &op2) {
    return op1.idx_ == op2.idx_ && op1.label_idx_ == op2.label_idx_ &&
      op1.type_ == op2.type_ && op1.options_ == op2.options_;
  }

 private:
  int idx_ = -1;
  int label_idx_ = -1;
  comm::RecvOptions options_;
  TransferType type_ = CLIENT;
};

class SharedMemData {
 public:
  SharedMemData(
      const SharedMemOptions& opts,
      const std::unordered_map<std::string, AnyP>& mem)
      : opts_(opts), mem_(mem) {}

  std::string info() const {
    std::stringstream ss;
    ss << opts_.info() << std::endl;
    ss << "Active batchsize: " << active_batch_size_ << std::endl;
    for (const auto& p : mem_) {
      ss << "[" << p.first << "]: " << p.second.info() << std::endl;
    }
    return ss.str();
  }

  size_t getEffectiveBatchSize() const {
    return active_batch_size_;
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

  // TODO maybe do something better here.
  std::unordered_map<std::string, AnyP>& GetMem() {
    return mem_;
  }
  const std::unordered_map<std::string, AnyP>& GetMem() const {
    return mem_;
  }
  void setEffectiveBatchSize(size_t bs) {
    active_batch_size_ = bs;
  }

  const SharedMemOptions& getSharedMemOptionsC() const {
    return opts_;
  }
  SharedMemOptions& getSharedMemOptions() {
    return opts_;
  }

  void setTimeout(int timeout_usec) {
    opts_.setTimeout(timeout_usec);
  }

  void setMinBatchSize(int minbatchsize) {
    opts_.setMinBatchSize(minbatchsize);
  }

  // Get a slide
  SharedMemData copySlice(int idx) const {
    SharedMemOptions opts = opts_;
    opts.setBatchSize(1);
    std::unordered_map<std::string, AnyP> mem;

    for (auto &p : mem_) {
      mem.insert(make_pair(p.first, p.second.getSlice(idx)));
    }

    return SharedMemData(opts, mem);
  }

 private:
  size_t active_batch_size_ = 0;
  SharedMemOptions opts_;
  std::unordered_map<std::string, AnyP> mem_;
};

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

namespace std
{
  template<> struct hash<elf::SharedMemOptions>
  {
    typedef elf::SharedMemOptions argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& s) const noexcept
    {
      result_type const h1 ( std::hash<int>{}(s.getIdx()) );
      result_type const h2 ( std::hash<int>{}(s.getLabelIdx()) );
      result_type const h3 ( std::hash<std::string>{}(s.getLabel()) );
      return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine (see Discussion)
    }
  };
}

