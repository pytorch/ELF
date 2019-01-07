/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "elf/comm/comm.h"
#include "elf/concurrency/ConcurrentQueue.h"
#include "elf/concurrency/Counter.h"
#include "sharedmem.h"
#include "elf/interface/game_client_interface.h"

// #define CONTEXT_DEBUG

#ifdef CONTEXT_DEBUG
#define C_PRINT(...) \
    std::cout << elf_utils::msec_since_epoch_from_now() << " [" << std::this_thread::get_id() << "] " << __VA_ARGS__ << std::endl;
#else
#define C_PRINT(...)
#endif

namespace elf {

class Collectors;
class CollectorContext;

class GameClient : public GameClientInterface {
 public:
  friend class Collectors;
  friend class CollectorContext;

  GameClient(Comm* comm, const Collectors* ctx)
      : context_(ctx),
        client_(comm->getClient()),
        n_(0),
        stop_games_(false),
        prepareToStop_(false) {}

  // For Game side.
  void start() override {
    n_++;
  }

  void End() override {
    numStoppedCounter_.increment();
  }

  bool DoStopGames() override {
    return stop_games_.load();
  }

  // TODO: This function should go away (ssengupta@fb)
  bool checkPrepareToStop() override {
    return prepareToStop_.load();
  }

  Binder getBinder() const override;

  comm::ReplyStatus sendWait(
      const std::vector<std::string>& targets,
      FuncsWithState* funcs) override {
    return client_->sendWait(funcs, targets);
  }

  comm::ReplyStatus sendBatchWait(
      const std::vector<std::string>& targets,
      const std::vector<FuncsWithState*>& funcs) override {
    return client_->sendBatchWait(funcs, targets);
  }

  comm::ReplyStatus sendBatchesWait(
      const std::vector<std::string>& targets,
      const std::vector<std::vector<FuncsWithState*>>& funcs,
      const std::vector<comm::SuccessCallback>& callbacks) override {
    return client_->sendBatchesWait(funcs, targets, callbacks);
  }

 private:
  const Collectors* context_;

  std::unique_ptr<Client> client_;

  std::atomic<int> n_;

  // TODO: This bools should go away (ssengupta@fb)
  std::atomic<bool> stop_games_;

  std::atomic<bool> prepareToStop_;

  concurrency::Counter<int> numStoppedCounter_;

  void prepareToStop() {
    prepareToStop_ = true;
  }

  void stopGames() {
    stop_games_ = true;
    numStoppedCounter_.waitUntilCount(n_);
  }
};

class Waiter {
 public:
  Waiter(
      const std::string& label,
      BatchServer* batch_server,
      const std::atomic_bool& done)
      : label_(label), batch_server_(batch_server), done_(done) {
    batch_server_->RegServer(label_);
  }

  const std::string& getLabel() const {
    return label_;
  }

  SharedMemData* wait(int time_usec = 0) {
    batch_server_->waitBatch(
        comm::RecvOptions(label_, 1, time_usec), &smem_batch_);
    if (smem_batch_.empty() || smem_batch_[0].data.empty()) {
      return nullptr;
    } else {
      return smem_batch_[0].data[0];
    }
  }

  void step(comm::ReplyStatus success = comm::SUCCESS) {
    // Finally we release the batch.
    // LOG(INFO) << "smem_batch_.size() = "
    //           << smem_batch_.size() << std::endl;
    // LOG(INFO) << hex << smem_batch_[0].from << ", "
    //           << smem_batch_[0].to << ", " << smem_batch_[0].m
    //           << dec << std::endl;
    batch_server_->ReleaseBatch(smem_batch_, success);
  }

  void finalize() {
    while (!done_.load()) {
      wait(2);
      step(comm::FAILED);
    }
  }

 private:
  std::string label_;
  BatchServer* batch_server_;
  std::vector<BatchMessage> smem_batch_;
  const std::atomic_bool& done_;
};

class GameStateCollector {
 public:
  using BatchCollectFunc = std::function<comm::ReplyStatus(SharedMemData*)>;

  GameStateCollector(
      std::unique_ptr<SharedMem>&& smem,
      BatchCollectFunc collect_func)
      : smem_(std::move(smem)), collect_func_(collect_func) {
    assert(smem_.get() != nullptr);
    assert(collect_func_ != nullptr);
  }

  SharedMemData& smemData() {
    return smem_->data();
  }
  SharedMem& smem() {
    return *smem_;
  }

  void start() {
    th_.reset(new std::thread([&]() {
      // assert(nice(10) == 10);
      collectAndSendBatch();
    }));
  }

  void prepareToStop() {
    msgQueue_.push(PREPARE_TO_STOP);
    completedSwitch_.waitUntilTrue();
    completedSwitch_.reset();
    // std::cout << " prepare to stop delivered "
    // << smem_->getSharedMemOptions().info() << std::endl;
  }

  void stop() {
    msgQueue_.push(STOP);
    completedSwitch_.waitUntilTrue();
    completedSwitch_.reset();
    th_->join();
  }

 private:
  enum _Msg { PREPARE_TO_STOP, STOP };

  std::unique_ptr<SharedMem> smem_;
  std::unique_ptr<std::thread> th_;

  BatchCollectFunc collect_func_ = nullptr;

  concurrency::Switch completedSwitch_;

  concurrency::ConcurrentQueue<_Msg> msgQueue_;

  // Collect game states into batch
  // Send batch to batch_server (through batchClient_)
  void collectAndSendBatch() {
    std::vector<Message> batch;

    // Initialize collector. For now just use 1.
    // Each collector has its own shared memory.
    // min_batchsize = 1 and wait indefinitely (timeout = 0).
    smem_->start();
    const std::string &label = smem_->data().getSharedMemOptions().getLabel();
    (void)label;

    while (true) {
      _Msg msg;
      if (msgQueue_.pop(&msg, std::chrono::microseconds(0))) {
        if (msg == PREPARE_TO_STOP) {
          // std::cout << " get prepare to stop signal "
          // << smem_opts.info() << std::endl;

          smem_->data().setMinBatchSize(0);
          smem_->data().setTimeout(2);
          completedSwitch_.set(true);
        } else if (msg == STOP) {
          completedSwitch_.set(true);
          break;
        }
      }

      C_PRINT("Receiver[" << label << "] Batch received. #batch = " << smem_->data().getEffectiveBatchSize());
      smem_->waitBatchFillMem();

      C_PRINT("Receiver[" << label << "] Batch received. #batch = " << smem_->data().getEffectiveBatchSize());
      comm::ReplyStatus batch_status = collect_func_(&smem_->data());

      C_PRINT("Receiver[" << label << "] Batch releasing. #batch = " << smem_->data().getEffectiveBatchSize());

      // LOG(INFO) << "Receiver: Release batch" << std::endl;
      smem_->waitReplyReleaseBatch(batch_status);
      C_PRINT("Receiver[" << label << "] Batch released. ");
    }
  }
};

class Collectors {
 public:
  using SharedMemFactory = std::function<std::unique_ptr<SharedMem>(
      const SharedMemOptions& opts,
      const std::unordered_map<std::string, AnyP>& mem)>;

  // For C side use only.
  Extractor& getExtractor() {
    return extractor_;
  }

  const Extractor& getExtractor() const {
    return extractor_;
  }

  size_t size() const {
    return collectors_.size();
  }

  void start() {
    for (auto& r : collectors_) {
      r->start();
    }
  }

  void prepareToStop() {
    for (auto& r : collectors_) {
      r->prepareToStop();
    }
  }

  void stop() {
    for (auto& r : collectors_) {
      r->stop();
    }
  }

  // Return (index of a string key, index within the string key).
  std::pair<int, int> getNextIdx(const std::string& label) const {
    auto it = smem2keys_.find(label);
    if (it == smem2keys_.end())
      return std::make_pair<int, int>(collectors_.size(), 0);
    else
      return std::make_pair<int, int>(
          (int)collectors_.size(), (int)it->second.indices_in_collectors.size());
  }

  SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys,
      SharedMemFactory smem_func,
      GameStateCollector::BatchCollectFunc collect_func) {
    // LOG(INFO) << "SharedMemInfo: " << info.info() << std::endl;
    // LOG(INFO) << "Keys: ";
    // for (const string &key : keys) {
    //    LOG(INFO) << key << " ";
    // }
    // std::cout << std::endl;
    const std::string& label = options.getRecvOptions().label;

    std::pair<int, int> nextIdx = getNextIdx(label);
    addKeys(label, keys);

    auto anyps = extractor_.getAnyP(keys);

    SharedMemOptions options_dup = options;
    options_dup.setIdx(nextIdx.first);
    options_dup.setLabelIdx(nextIdx.second);

    collectors_.emplace_back(
        new GameStateCollector(smem_func(options_dup, anyps), collect_func));

    return collectors_.back()->smemData();
  }

  const std::vector<std::string>* getSMemKeys(
      const std::string& smem_name) const {
    auto it = smem2keys_.find(smem_name);

    if (it == smem2keys_.end()) {
      return nullptr;
    }

    return &it->second.keys;
  }

  SharedMem& getSMem(int idx) {
    return collectors_[idx]->smem();
  }

  SharedMem& pickSMem(const std::string &label, std::mt19937 *rng) {
    auto it = smem2keys_.find(label);
    assert(it != smem2keys_.end());
    const auto &indices = it->second.indices_in_collectors;
    int idx = (*rng)() % indices.size();
    return getSMem(indices[idx]);
  }

 private:
  struct _KeyInfo {
    std::vector<int> indices_in_collectors;
    std::vector<std::string> keys;
  };

  Extractor extractor_;
  std::vector<std::unique_ptr<GameStateCollector>> collectors_;
  std::unordered_map<std::string, _KeyInfo> smem2keys_;

  void addKeys(const std::string& label, const std::vector<std::string>& keys) {
    _KeyInfo& info = smem2keys_[label];
    info.keys = keys;
    info.indices_in_collectors.push_back(collectors_.size());
  }
};

class CollectorContext {
 public:
  using GameCallback = std::function<void(int game_idx, GameClient*)>;

  CollectorContext() {
    // Wait for the derived class to add entries to extractor_.
    collectors_.reset(new Collectors);
    server_ = comm_.getServer();
    client_.reset(new GameClient(&comm_, collectors_.get()));
  }

  GameClient* getClient() {
    return client_.get();
  }

  Collectors* getCollectors() {
    return collectors_.get();
  }

  void setStartCallback(int num_games, GameCallback cb) {
    num_games_ = num_games;
    game_cb_ = cb;
  }

  void setCBAfterGameStart(std::function<void()> cb) {
    cb_after_game_start_ = cb;
  }

  void start() {
    collectors_->start();
    server_->waitForRegs(collectors_->size());

    game_threads_.clear();
    auto* client = getClient();
    for (int i = 0; i < num_games_; ++i) {
      game_threads_.emplace_back([i, client, this]() {
        // assert(nice(19) == 19);
        client->start();
        game_cb_(i, client);
        client->End();
      });
    }

    if (cb_after_game_start_ != nullptr) {
      cb_after_game_start_();
    }
  }

  void stop() {
    std::cout << "Prepare to stop ..." << std::endl;
    client_->prepareToStop();

    // First set the timeout for all collectors to be finite number.
    collectors_->prepareToStop();

    // Then stop all the threads.
    std::cout << "Stop all game threads ..." << std::endl;
    client_->stopGames();

    std::cout << "All games sent notification, "
              << "Waiting until they join" << std::endl;

    for (auto& p : game_threads_) {
      p.join();
    }

    std::cout << "Stop all collectors ..." << std::endl;
    collectors_->stop();
  }

  SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys,
      GameStateCollector::BatchCollectFunc collect_func) {
    auto creator = [&](const SharedMemOptions& options,
                       const std::unordered_map<std::string, AnyP>& anyps) {
      return std::unique_ptr<SharedMemLocal>(
          new SharedMemLocal(server_.get(), options, anyps));
    };

    return collectors_->allocateSharedMem(options, keys, creator, collect_func);
  }

 private:
  Comm comm_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<GameClient> client_;
  std::unique_ptr<Collectors> collectors_;

  int num_games_ = 0;
  GameCallback game_cb_ = nullptr;
  std::function<void()> cb_after_game_start_ = nullptr;
  std::vector<std::thread> game_threads_;
};

class BatchContext {
 public:
  BatchContext() : done_(false) {
    batch_server_ = batch_comm_.getServer();
    batchClient_ = batch_comm_.getClient();
  }

  void start() {
    batch_server_->waitForRegs(waiters_.size());
  }

  BatchClient* getClient() {
    return batchClient_.get();
  }

  Waiter* getWaiter(std::string new_wait_label = "") {
    auto id = std::this_thread::get_id();
    auto it = waiters_.find(id);
    if (it == waiters_.end()) {
      auto insert_info = waiters_.emplace(make_pair(
          id, new Waiter(new_wait_label, batch_server_.get(), done_)));
      it = insert_info.first;
    }

    return it->second.get();
  }

  void stop(CollectorContext* context) {
    // We need to stop everything.
    // Assuming that all games will be constantly sending states.
    std::thread tmp_thread([&]() {
      // assert(nice(10) == 10);
      if (context != nullptr)
        context->stop();
      std::cout << "Stop tmp pool..." << std::endl;
      done_ = true;
    });

    auto it = waiters_.find(std::this_thread::get_id());
    if (it != waiters_.end()) {
      it->second->finalize();
    }

    tmp_thread.join();
  }

 private:
  BatchComm batch_comm_;
  std::unique_ptr<BatchServer> batch_server_;
  std::unique_ptr<BatchClient> batchClient_;

  std::unordered_map<std::thread::id, std::unique_ptr<Waiter>> waiters_;
  std::atomic_bool done_;
};


inline Binder GameClient::getBinder() const {
  return Binder(context_->getExtractor(),
      [&](const std::string &name) { return context_->getSMemKeys(name); });
}

} // namespace elf
