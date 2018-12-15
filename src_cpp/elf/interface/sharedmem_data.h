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

#include "extractor.h"

namespace elf {

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
