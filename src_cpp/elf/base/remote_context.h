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

namespace elf {

using json = nlohmann::json;
static constexpr int kPortPerClient = 2;

template <typename T>
using Queue = elf::concurrency::ConcurrentQueueMoodyCamel<T>;

inline std::string timestr() {
  return std::to_string(elf_utils::msec_since_epoch_from_now());
}

class ReplyQs {
 public:
  using Q = Queue<std::string>; 
  ReplyQs() : signature_(std::to_string(time(NULL))) {
  }

  int addQueue() {
    reply_qs_.emplace_back(new Q);
    return reply_qs_.size() - 1;
  }

  void push(const std::string &msg) {
    // std::cout << timestr() << ", Get reply... about to parse" << std::endl;
    json j = json::parse(msg);

    // std::cout << timestr() << "Get reply... #record: " << j.size() <<
    // std::endl;
    for (const auto& jj : j) {
      if (checkSignature(jj)) {
        // std::cout << "Get reply... idx: " << jj["idx"] << "/" << reply_qs_.size() << std::endl;
        reply_qs_[jj["idx"]]->push(jj["content"]);
      }
    }
  }

  void pop(int idx, std::string *msg) {
    reply_qs_[idx]->pop(msg);
  }

  const std::string& getSignature() const { return signature_; }

 private:
  std::vector<std::unique_ptr<Q>> reply_qs_;
  const std::string signature_;

  bool checkSignature(const json& j) {
    bool has_signature = j.find("signature") != j.end();
    if (!has_signature || j["signature"] != signature_) {
      std::cout << "Invalid signature! ";
      if (has_signature) std::cout << " get: " << j["signature"];
      std::cout << " expect: " << signature_;
      return false;
    } else {
      return true;
    }
  }
};

class BatchSenderInstance {
 public:
  BatchSenderInstance(const elf::shared::Options& netOptions, ReplyQs *reply_qs) 
    : reply_qs_(reply_qs) {
    assert(reply_qs_ != nullptr);
    auto netOptions2 = netOptions; 
    netOptions2.usec_sleep_when_no_msg = 10;
    // Not used.
    netOptions2.usec_resend_when_no_msg = 10000;
    netOptions2.verbose = false;

    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    // netOptions.verbose = true;
    server_.reset(new msg::Server(netOptions2));
  }

  void push(const std::string &msg) {
    q_.push(msg);
  }

  void start() {
    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      (void)identity;
      // Send request
      // std::cout << timestr() << ", Send request .." << std::endl;
      return q_.pop(reply_msg, std::chrono::milliseconds(0));
    };

    auto proc = [&](const std::string& identity, const std::string& recv_msg) {
      (void)identity;
      reply_qs_->push(recv_msg);
      return true;
    };

    server_->setCallbacks(proc, replier);
    server_->start();
  }

 private:
  std::unique_ptr<msg::Server> server_;
  Queue<std::string> q_;
  ReplyQs *reply_qs_ = nullptr;
};

class BatchSenderInstances {
 public:
  BatchSenderInstances(const elf::shared::Options &netOptions, ReplyQs *reply_qs) 
    : netOptions_(netOptions), reply_qs_(reply_qs), rng_(time(NULL)) {}

  int addServer(const std::string &identity) {
    json info;
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = servers_.find(identity);
    if (it == servers_.end()) {
      auto netOptions = netOptions_;
      netOptions.port += servers_.size();
      server_ids_.push_back(identity);
      auto &server = servers_[identity];
      server.reset(new BatchSenderInstance(netOptions, reply_qs_)); 
      server->start();

      std::cout << timestr() << ", New connection from " << identity 
        << ", assigned port: " << netOptions.port << std::endl;

      return netOptions.port;
    } else {
      return -1;
    }
  }

  void send(const std::string &msg) {
    BatchSenderInstance *q = nullptr;

    while (server_ids_.empty()) {
      std::cout << "No server available, wait for 10s" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // std::cout << "get a server to send ... " << std::endl;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      int idx = rng_() % server_ids_.size();
      q = servers_[server_ids_[idx]].get();
    }

    // std::cout << "sending message ... " << std::endl;

    assert(q);
    q->push(msg);
  }

 private:
  const elf::shared::Options netOptions_;
  ReplyQs *reply_qs_ = nullptr;

  std::mutex mutex_;
  std::mt19937 rng_;
  std::vector<std::string> server_ids_;
  std::unordered_map<std::string, std::unique_ptr<BatchSenderInstance>> servers_;
};

class BatchSender : public GameContext {
 public:
  BatchSender(const Options& options, const msg::Options& net)
      : GameContext(options) {
    auto netOptions = msg::getNetOptions(options, net);
    netOptions.usec_sleep_when_no_msg = 10;
    // Not used.
    netOptions.usec_resend_when_no_msg = 10000;
    netOptions.verbose = false;

    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    // netOptions.verbose = true;
    ctrl_server_.reset(new msg::Server(netOptions));
    
    netOptions.port ++;
    servers_.reset(new BatchSenderInstances(netOptions, &reply_qs_));
  }

  void start() override {
    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      json info;
      info["valid"] = true;
      info["signature"] = reply_qs_.getSignature();
      for (int i = 0; i < kPortPerClient; ++i) {
        int port = servers_->addServer(identity + "_" + std::to_string(i));
        if (port == -1) {
          info["valid"] = false;
          break;
        }
        info["port"].push_back(port);
      }

      *reply_msg = info.dump();
      return true;
    };

    auto proc = [&](const std::string& identity, const std::string& recv_msg) {
      (void)identity;
      (void)recv_msg;
      return true;
    };

    ctrl_server_->setCallbacks(proc, replier);
    ctrl_server_->start();

    GameContext::start();
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
      int reply_idx = reply_qs_.addQueue();

      func = [this, reply_idx](SharedMemData* smem_data) {
        json j;
        elf::SMemToJson(*smem_data, input_keys_, j);
        
        servers_->send(j.dump());
        std::string reply;
        reply_qs_.pop(reply_idx, &reply);
        // std::cout << ", got reply_j: "<< std::endl;
        elf::SMemFromJson(json::parse(reply), *smem_data);
        // std::cout << ", after parsing smem: "<< std::endl;
        return comm::SUCCESS;
      };
    }

    return getCollectorContext()->allocateSharedMem(options, keys, func);
  }

 private:
  std::unique_ptr<msg::Server> ctrl_server_;
  std::unique_ptr<BatchSenderInstances> servers_;
  ReplyQs reply_qs_;

  std::set<std::string> remote_labels_;
  std::set<std::string> input_keys_ {"s", "hash"};
};

class ReplyMsgs {
 public:
  void setSignature(const std::string& signature) {
    signature_ = signature;
  }

  void add(int idx, std::string&& s) {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    json j;
    j["idx"] = idx;
    j["signature"] = signature_;
    j["content"] = s;
    j_.push_back(j);
  }
  size_t size() {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    return j_.size();
  }

  std::string dump() {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    /*
    if (j_.size() > 0) {
      std::cout << "dumping.. #records: " << j_.size() << std::endl;
    }
    */
    std::string ret = j_.dump();
    j_.clear();
    return ret;
  }

 private:
  std::mutex reply_mutex_;
  std::string signature_;
  json j_;
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

class SharedMemRemote : public SharedMem {
 public:
  SharedMemRemote(
      const SharedMemOptions& opts,
      const std::unordered_map<std::string, AnyP>& mem,
      const int local_idx,
      const int local_label_idx,
      ReplyMsgs* reply_q, Stats *stats)
      : SharedMem(opts, mem),
        local_idx_(local_idx),
        local_label_idx_(local_label_idx),
        reply_q_(reply_q),
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
    assert(reply_q_ != nullptr);

    auto& opt = smem_.getSharedMemOptions();
    opt.setIdx(remote_idx_);
    opt.setLabelIdx(remote_label_idx_);

    if (stats_ != nullptr) {
      stats_->recordRelease(smem_.getEffectiveBatchSize());
    }

    json j;
    elf::SMemToJsonExclude(smem_, input_keys_, j);
    reply_q_->add(remote_label_idx_, j.dump());
  }

 private:
  const int local_idx_;
  const int local_label_idx_;

  int remote_idx_ = -1;
  int remote_label_idx_ = -1;

  Queue<std::string> q_;
  std::set<std::string> input_keys_{"s", "hash"};
  ReplyMsgs* reply_q_ = nullptr;
  Stats *stats_ = nullptr;
};

class BatchReceiverInstance {
 public:
   BatchReceiverInstance(Collectors* collectors)
     : collectors_(collectors), rng_(time(NULL)) {
     assert(collectors_ != nullptr);
   }

   void start(const std::string &signature, const elf::shared::Options &netOptions) {
    client_.reset(new msg::Client(netOptions));
    reply_.setSignature(signature);

    auto receiver = [&](const std::string& recv_msg) -> int64_t {
      // Get data
      int idx = rng_() % collectors_->size();
      // std::cout << timestr() << ", Forward data to " << idx << "/" <<
      //     collectors_->size() << std::endl;
      dynamic_cast<SharedMemRemote&>(collectors_->getSMem(idx))
          .push(recv_msg);
      return -1;
    };

    auto sender = [&]() {
      // std::cout << timestr() << ", Dump data" << std::endl;
      return reply_.dump();
    };

    client_->setCallbacks(sender, receiver);
    client_->start();
   }

   ReplyMsgs &getReplyMsgs() { return reply_; }

 private:
   std::unique_ptr<msg::Client> client_;

   Collectors *collectors_ = nullptr;
   ReplyMsgs reply_;

   std::mt19937 rng_;
};


class BatchReceiver : public GCInterface {
 public:
  BatchReceiver(const Options& options, const msg::Options& net)
      : GCInterface(options), netOptions_(net) {
    batchContext_.reset(new BatchContext);
    auto netOptions = msg::getNetOptions(options, net);
    netOptions.usec_sleep_when_no_msg = 1000000;
    netOptions.usec_resend_when_no_msg = -1;
    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    netOptions.verbose = false;
    
    ctrl_client_.reset(new msg::Client(netOptions));
    collectors_.reset(new Collectors);

    for (size_t i = 0; i < kPortPerClient; ++i) {
      clients_.emplace_back(new BatchReceiverInstance(collectors_.get()));
    }
  }

  void start() override {
    batchContext_->start();
    
    auto receiver = [&](const std::string& recv_msg) -> int64_t {
      // Get data
      json j = json::parse(recv_msg);
      if (!j["valid"]) return -1;

      assert(j["port"].size() == kPortPerClient);

      auto netOptions = msg::getNetOptions(this->options(), netOptions_);
      netOptions.usec_sleep_when_no_msg = 1000;
      netOptions.usec_resend_when_no_msg = 10;
      // netOptions.msec_sleep_when_no_msg = 2000;
      // netOptions.msec_resend_when_no_msg = 2000;
      netOptions.verbose = false;

      for (size_t i = 0; i < kPortPerClient; ++i) {
        netOptions.port = j["port"][i];
        clients_[i]->start(j["signature"], netOptions);
      }
      return -1;
    };

    auto sender = [&]() {
      return "";
    };

    ctrl_client_->setCallbacks(sender, receiver);
    ctrl_client_->start();
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
    int client_idx = idx.second % clients_.size(); 
    // std::cout << "client_idx: " << client_idx << std::endl;
    BatchReceiverInstance *receiver = clients_[client_idx].get();

    auto creator = [&, idx, receiver](
                       const SharedMemOptions& options,
                       const std::unordered_map<std::string, AnyP>& anyps) {
      return std::unique_ptr<SharedMemRemote>(
          new SharedMemRemote(options, anyps, idx.first, idx.second, &receiver->getReplyMsgs(), &stats_));
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
  std::unique_ptr<msg::Client> ctrl_client_;
  std::vector<std::unique_ptr<BatchReceiverInstance>> clients_;

  const msg::Options netOptions_;
  Stats stats_;
};

} // namespace elf
