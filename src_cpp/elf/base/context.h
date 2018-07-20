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
#include "extractor.h"
#include "sharedmem.h"

namespace elf {

class Context;

class GameClient {
 public:
  friend class Context;

  GameClient(Comm* comm, const Context* ctx)
      : context_(ctx),
        client_(comm->getClient()),
        n_(0),
        stop_games_(false),
        prepareToStop_(false) {}

  // For Game side.
  void start() {
    n_++;
  }

  void End() {
    numStoppedCounter_.increment();
  }

  bool DoStopGames() {
    return stop_games_.load();
  }

  // TODO: This function should go away (ssengupta@fb)
  bool checkPrepareToStop() {
    return prepareToStop_.load();
  }

  template <typename S>
  FuncsWithState BindStateToFunctions(
      const std::vector<std::string>& smem_names,
      S* s);

  template <typename S>
  std::vector<FuncsWithState> BindStateToFunctions(
      const std::vector<std::string>& smem_names,
      const std::vector<S*>& batch_s);

  comm::ReplyStatus sendWait(
      const std::vector<std::string>& targets,
      FuncsWithState* funcs) {
    return client_->sendWait(funcs, targets);
  }

  comm::ReplyStatus sendBatchWait(
      const std::vector<std::string>& targets,
      const std::vector<FuncsWithState*>& funcs) {
    return client_->sendBatchWait(funcs, targets);
  }

 private:
  const Context* context_;

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

class Context {
 private:
  class GameStateCollector {
   public:
    GameStateCollector(
        Server* server,
        BatchClient* batchClient,
        std::unique_ptr<SharedMem>&& smem)
        : server_(server), batchClient_(batchClient), smem_(std::move(smem)) {
      assert(smem_.get() != nullptr);
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

    Server* server_;
    BatchClient* batchClient_;
    std::unique_ptr<SharedMem> smem_;
    std::unique_ptr<std::thread> th_;

    concurrency::Switch completedSwitch_;

    concurrency::ConcurrentQueue<_Msg> msgQueue_;

    // Collect game states into batch
    // Send batch to batch_server (through batchClient_)
    void collectAndSendBatch() {
      std::vector<Message> batch;

      // Initialize collector. For now just use 1.
      // Each collector has its own shared memory.
      // min_batchsize = 1 and wait indefinitely (timeout = 0).
      const SharedMemOptions& smem_opts = smem_->getSharedMemOptions();
      server_->RegServer(smem_opts.getRecvOptions().label);

      while (true) {
        _Msg msg;
        if (msgQueue_.pop(&msg, std::chrono::microseconds(0))) {
          if (msg == PREPARE_TO_STOP) {
            // std::cout << " get prepare to stop signal "
            // << smem_opts.info() << std::endl;

            smem_->setMinBatchSize(0);
            smem_->setTimeout(2);
            completedSwitch_.set(true);
          } else if (msg == STOP) {
            completedSwitch_.set(true);
            break;
          }
        }
        smem_->waitBatchFillMem(server_);
        // std::cout << "Receiver[" << smem_opts.getLabel() << "] Batch
        // received. #batch = "
        //          << smem_->getEffectiveBatchSize() << std::endl;

        comm::ReplyStatus batch_status =
            batchClient_->sendWait(smem_.get(), {""});

        // std::cout << "Receiver[" << smem_opts.getLabel() << "] Batch
        // releasing. #batch = "
        //          << smem_->getEffectiveBatchSize() << std::endl;

        // LOG(INFO) << "Receiver: Release batch" << std::endl;
        smem_->waitReplyReleaseBatch(server_, batch_status);
      }
    }
  };

 public:
  using GameCallback = std::function<void(int game_idx, GameClient*)>;

  Context() {
    // Wait for the derived class to add entries to extractor_.
    server_ = comm_.getServer();
    client_.reset(new GameClient(&comm_, this));

    batch_server_ = batch_comm_.getServer();
    batchClient_ = batch_comm_.getClient();
  }

  // For C side use only.
  Extractor& getExtractor() {
    return extractor_;
  }

  const Extractor& getExtractor() const {
    return extractor_;
  }

  GameClient* getClient() {
    return client_.get();
  }

  void setStartCallback(int num_games, GameCallback cb) {
    num_games_ = num_games;
    game_cb_ = cb;
  }

  void setCBAfterGameStart(std::function<void()> cb) {
    cb_after_game_start_ = cb;
  }

  // Initialization
  SharedMemOptions createSharedMemOptions(
      const std::string& name,
      int batchsize) {
    return SharedMemOptions(name, batchsize);
  }

  SharedMem& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys) {
    // LOG(INFO) << "SharedMemInfo: " << info.info() << std::endl;
    // LOG(INFO) << "Keys: ";
    // for (const string &key : keys) {
    //    LOG(INFO) << key << " ";
    // }
    // std::cout << std::endl;

    smem2keys_[options.getRecvOptions().label] = keys;
    auto anyps = extractor_.getAnyP(keys);

    collectors_.emplace_back(new GameStateCollector(
        server_.get(),
        batchClient_.get(),
        std::unique_ptr<SharedMem>(
            new SharedMem(collectors_.size(), options, anyps))));
    return collectors_.back()->smem();
  }

  const std::vector<std::string>* getSMemKeys(
      const std::string& smem_name) const {
    auto it = smem2keys_.find(smem_name);

    if (it == smem2keys_.end()) {
      return nullptr;
    }

    return &it->second;
  }

  void start() {
    for (auto& r : collectors_) {
      r->start();
    }
    server_->waitForRegs(collectors_.size());

    batch_server_->RegServer("");
    batch_server_->waitForRegs(1);

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

  std::string version() const {
#ifdef GIT_COMMIT_HASH
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
    return TOSTRING(GIT_COMMIT_HASH) "_" TOSTRING(GIT_STAGED);
#else
    return "";
#endif
#undef STRINGIFY
#undef TOSTRING
  }

  const SharedMem* wait(int time_usec = 0) {
    batch_server_->waitBatch(comm::RecvOptions("", 1, time_usec), &smem_batch_);
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

  void stop() {
    // We need to stop everything.
    // Assuming that all games will be constantly sending states.
    std::atomic<bool> tmp_thread_done(false);

    std::thread tmp_thread([&]() {
      // assert(nice(10) == 10);

      std::cout << "Prepare to stop ..." << std::endl;
      client_->prepareToStop();

      // First set the timeout for all collectors to be finite number.
      for (auto& r : collectors_) {
        r->prepareToStop();
      }

      // Then stop all the threads.
      std::cout << "Stop all game threads ..." << std::endl;
      client_->stopGames();

      std::cout << "All games sent notification, "
                << "Waiting until they join" << std::endl;

      for (auto& p : game_threads_) {
        p.join();
      }

      std::cout << "Stop all collectors ..." << std::endl;
      for (auto& r : collectors_) {
        r->stop();
      }

      std::cout << "Stop tmp pool..." << std::endl;
      tmp_thread_done = true;
    });

    while (!tmp_thread_done) {
      wait(2);
      step(comm::FAILED);
    }

    tmp_thread.join();
  }

 private:
  Extractor extractor_;
  std::vector<std::unique_ptr<GameStateCollector>> collectors_;

  Comm comm_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<GameClient> client_;

  BatchComm batch_comm_;
  std::unique_ptr<BatchServer> batch_server_;
  std::unique_ptr<BatchClient> batchClient_;

  std::vector<BatchMessage> smem_batch_;

  std::unordered_map<std::string, std::vector<std::string>> smem2keys_;

  int num_games_ = 0;
  GameCallback game_cb_ = nullptr;
  std::function<void()> cb_after_game_start_ = nullptr;
  std::vector<std::thread> game_threads_;
};

template <typename S>
inline FuncsWithState GameClient::BindStateToFunctions(
    const std::vector<std::string>& smem_names,
    S* s) {
  const Extractor& extractor = context_->getExtractor();
  FuncsWithState funcsWithState;

  std::set<std::string> dup;

  for (const auto& name : smem_names) {
    const std::vector<std::string>* keys = context_->getSMemKeys(name);

    if (keys == nullptr) {
      continue;
    }

    for (const auto& key : *keys) {
      if (dup.find(key) != dup.end()) {
        continue;
      }

      const FuncMapBase* funcs = extractor.getFunctions(key);

      if (funcs == nullptr) {
        continue;
      }

      if (funcsWithState.state_to_mem_funcs.addFunction(
              key, funcs->BindStateToStateToMemFunc(*s))) {
        // LOG(INFO) << "GetPackage: key: " << key << "Add s2m "
        //           << std:: endl;
      }

      if (funcsWithState.mem_to_state_funcs.addFunction(
              key, funcs->BindStateToMemToStateFunc(*s))) {
        // LOG(INFO) << "GetPackage: key: " << key << "Add m2s "
        //           << std::endl;
      }
      dup.insert(key);
    }
  }
  return funcsWithState;
}

template <typename S>
inline std::vector<FuncsWithState> GameClient::BindStateToFunctions(
    const std::vector<std::string>& smem_names,
    const std::vector<S*>& batch_s) {
  const Extractor& extractor = context_->getExtractor();
  std::vector<FuncsWithState> batchFuncsWithState(batch_s.size());

  std::set<std::string> dup;

  for (const auto& name : smem_names) {
    const std::vector<std::string>* keys = context_->getSMemKeys(name);

    if (keys == nullptr) {
      continue;
    }

    for (const auto& key : *keys) {
      if (dup.find(key) != dup.end()) {
        continue;
      }

      const FuncMapBase* funcs = extractor.getFunctions(key);

      if (funcs == nullptr) {
        continue;
      }

      for (size_t i = 0; i < batch_s.size(); ++i) {
        auto& funcsWithState = batchFuncsWithState[i];
        S* s = batch_s[i];

        if (funcsWithState.state_to_mem_funcs.addFunction(
                key, funcs->BindStateToStateToMemFunc(*s))) {
          // LOG(INFO) << "GetPackage: key: " << key << "Add s2m "
          //           << std:: endl;
        }

        if (funcsWithState.mem_to_state_funcs.addFunction(
                key, funcs->BindStateToMemToStateFunc(*s))) {
          // LOG(INFO) << "GetPackage: key: " << key << "Add m2s "
          //           << std::endl;
        }
      }
      dup.insert(key);
    }
  }
  return batchFuncsWithState;
}

} // namespace elf
