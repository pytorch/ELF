/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// TODO: Figure out how to remove this (ssengupta@fb)
#include <time.h>

#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "elf/base/sharedmem_serializer.h"
#include "elf/distributed/addrs.h"
#include "elf/distributed/shared_rw_buffer3.h"
#include "game_context.h"
#include "game_interface.h"

#include "remote_sender.h"
#include "remote_receiver.h"

namespace elf {

using json = nlohmann::json;

class BatchSender : public elf::remote::RemoteSender {
 public:
  BatchSender(const Options& options, const msg::Options& net) 
    : elf::remote::RemoteSender(options, net, elf::remote::RAND_ONE) {
  }
  void setRemoteLabels(const std::set<std::string>& remote_labels) {
    remote_labels_ = remote_labels;
  }

  SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys) override {
    GameStateCollector::BatchCollectFunc func = nullptr;

    const std::string& label = options.getRecvOptions().label;

    auto it = remote_labels_.find(label);
    if (it == remote_labels_.end()) {
      BatchClient* batch_client = getBatchContext()->getClient();
      func = [batch_client](SharedMemData* smem_data) {
        return batch_client->sendWait(smem_data, {""});
      };
    } else {
      // Send to the client and wait for its response.
      int reply_idx = addQueue();

      func = [this, reply_idx](SharedMemData* smem_data) {
        json j;
        elf::SMemToJson(*smem_data, input_keys_, j);
        
        sendToClient(j.dump());
        std::string reply;
        getFromClient(reply_idx, &reply);
        // std::cout << ", got reply_j: "<< std::endl;
        elf::SMemFromJson(json::parse(reply), *smem_data);
        // std::cout << ", after parsing smem: "<< std::endl;
        return comm::SUCCESS;
      };
    }

    return getCollectorContext()->allocateSharedMem(options, keys, func);
  }

private:
  std::set<std::string> remote_labels_;
  std::set<std::string> input_keys_ {"s", "hash"};
};

class Stats {
 public:
  void feed(int idx, int batchsize) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_[idx] ++;
    sum_batchsize_ += batchsize;
    total_batchsize_ += batchsize;
    count_ ++;
    if (count_ >= 5000) {
      int min_val = std::numeric_limits<int>::max();
      int max_val = -std::numeric_limits<int>::max();
      for (const auto &p : stats_) {
        if (p.first > max_val) max_val = p.first;
        if (p.first < min_val) min_val = p.first;
      }
      // std::cout << elf_utils::now() << " Key range: [" << min_val << ", " << max_val << "]: ";
      std::vector<int> zero_entries;
      for (int i = min_val; i <= max_val; ++i) {
        // std::cout << stats_[i] << ",";
        if (stats_[i] == 0) zero_entries.push_back(i);
      }
      if (!zero_entries.empty()) {
        std::cout << elf_utils::now() << ", zero entry: ";
        for (const auto &entry : zero_entries) {
          std::cout << entry << ",";
        }
        std::cout << std::endl;
      }

      std::cout << elf_utils::now() << " Avg batchsize: " 
        << static_cast<float>(sum_batchsize_) / count_ 
        << ", #sample: " << total_batchsize_ 
        << ", #replied: " << total_release_batchsize_ 
        << ", #in_queue: " << total_batchsize_ - total_release_batchsize_ 
        << std::endl;

      reset();
    }
  }

  void recordRelease(int batchsize) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_release_batchsize_ += batchsize;
  }

 private:
  std::mutex mutex_;
  std::unordered_map<int, int> stats_;
  int count_ = 0;
  int sum_batchsize_ = 0;

  int total_batchsize_ = 0;
  int total_release_batchsize_ = 0;

  void reset() {
    stats_.clear();
    count_ = 0;
    sum_batchsize_ = 0;
  }
};

using ReplyRecvFunc = std::function<void (int, std::string &&)>;

class SharedMemRemote : public SharedMem {
 public:
  SharedMemRemote(
      const SharedMemOptions& opts,
      const std::unordered_map<std::string, AnyP>& mem,
      const int local_idx,
      const int local_label_idx,
      ReplyRecvFunc reply_recv, Stats *stats)
      : SharedMem(opts, mem),
        local_idx_(local_idx),
        local_label_idx_(local_label_idx),
        reply_recv_(reply_recv),
        stats_(stats) {}

  void start() override {}

  void push(const std::string& msg) {
    q_.push(msg);
  }

  void waitBatchFillMem() override {
    std::string msg;
    q_.pop(&msg);
    elf::SMemFromJson(json::parse(msg), smem_);

    auto& opt = smem_.getSharedMemOptions();
    remote_idx_ = opt.getIdx();
    remote_label_idx_ = opt.getLabelIdx();

    if (stats_ != nullptr) 
      stats_->feed(remote_label_idx_, smem_.getEffectiveBatchSize());

    opt.setIdx(local_idx_);
    opt.setLabelIdx(local_label_idx_);
  }

  void waitReplyReleaseBatch(comm::ReplyStatus batch_status) override {
    (void)batch_status;
    assert(reply_recv_ != nullptr);

    auto& opt = smem_.getSharedMemOptions();
    opt.setIdx(remote_idx_);
    opt.setLabelIdx(remote_label_idx_);

    if (stats_ != nullptr) {
      stats_->recordRelease(smem_.getEffectiveBatchSize());
    }

    json j;
    elf::SMemToJsonExclude(smem_, input_keys_, j);
    reply_recv_(remote_label_idx_, j.dump());
  }

 private:
  const int local_idx_;
  const int local_label_idx_;

  int remote_idx_ = -1;
  int remote_label_idx_ = -1;

  remote::Queue<std::string> q_;
  std::set<std::string> input_keys_{"s", "hash"};
  ReplyRecvFunc reply_recv_ = nullptr;
  Stats *stats_ = nullptr;
};

class BatchReceiver : public elf::remote::RemoteReceiver {
 public:
  BatchReceiver(const Options& options, const msg::Options& net) 
    : elf::remote::RemoteReceiver(options, net), rng_(time(NULL)) {
    batchContext_.reset(new BatchContext);
    collectors_.reset(new Collectors);

    auto recv_func = [&](const std::string &msg) {
      int idx = rng_() % collectors_->size();
      // std::cout << timestr() << ", Forward data to " << idx << "/" <<
      //     collectors_->size() << std::endl;
      dynamic_cast<SharedMemRemote&>(collectors_->getSMem(idx))
          .push(msg);
    };

    initClients(recv_func);
  }

  void start() override {
    batchContext_->start();
    elf::remote::RemoteReceiver::start();
    collectors_->start();
  }

  void stop() override {
    batchContext_->stop(nullptr);
  }

  SharedMemData* wait(int time_usec = 0) override {
    return batchContext_->getWaiter()->wait(time_usec);
  }

  void step(comm::ReplyStatus success = comm::SUCCESS) override {
    return batchContext_->getWaiter()->step(success);
  }

  SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys) override {
    const std::string& label = options.getRecvOptions().label;

    // Allocate data.
    auto idx = collectors_->getNextIdx(label);
    int client_idx = idx.second % getNumClients();
    // std::cout << "client_idx: " << client_idx << std::endl;

    auto reply_func = [&, client_idx](int idx, std::string &&msg) {
      addReplyMsg(client_idx, idx, std::move(msg));
    };

    auto creator = [&, idx, reply_func](
                       const SharedMemOptions& options,
                       const std::unordered_map<std::string, AnyP>& anyps) {
      return std::unique_ptr<SharedMemRemote>(
          new SharedMemRemote(options, anyps, idx.first, idx.second, reply_func, &stats_));
    };

    BatchClient* batch_client = batchContext_->getClient();
    auto collect_func = [batch_client](SharedMemData* smem_data) {
      return batch_client->sendWait(smem_data, {""});
    };

    return collectors_->allocateSharedMem(options, keys, creator, collect_func);
  }

  Extractor& getExtractor() override {
    return collectors_->getExtractor();
  }

private:
  std::unique_ptr<BatchContext> batchContext_;
  std::unique_ptr<Collectors> collectors_;
  std::mt19937 rng_;
  Stats stats_;
};

} // namespace elf
