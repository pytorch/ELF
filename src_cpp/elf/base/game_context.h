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
#include <vector>

#include "context.h"
#include "elf/logging/IndexedLoggerFactory.h"
#include "elf/interface/game_interface.h"

#include <thread>

namespace elf {

class GameContext : public GCInterface {
 public:
  GameContext(const Options& options)
      : GCInterface(options), 
        logger_(elf::logging::getLogger("elf::GameContext-", "")) {
    std::cout << "Initialize game context" << std::endl;
    batchContext_.reset(new BatchContext());
    collectorContext_.reset(new CollectorContext());

    client_ = collectorContext_->getClient();

    for (int i = 0; i < options.num_game_thread; ++i) {
      game::Options opt;
      opt.game_idx = i;
      opt.seed = 0;
      opt.verbose = options.verbose;
      opt.job_id = options.job_id;
      games_.emplace_back(new game::Base(client_, opt));
    }

    collectorContext_->setStartCallback(
        options_.num_game_thread,
        [this](int i, elf::GameClient*) { games_[i]->mainLoop(); });
  }

  // Virtual functions for python.
  void start() override {
    collectorContext_->start();
    batchContext_->start();
  }

  void stop() override {
    batchContext_->stop(collectorContext_.get());
  }

  GameClientInterface *getClient() override { return client_; }

  SharedMemData* wait(int time_usec = 0) override {
    return batchContext_->getWaiter()->wait(time_usec);
  }

  void step(comm::ReplyStatus success = comm::SUCCESS) override {
    return batchContext_->getWaiter()->step(success);
  }

  SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys) override {
    BatchClient* batch_client = batchContext_->getClient();

    auto collect_func = [batch_client](SharedMemData* smem_data) {
      return batch_client->sendWait(smem_data, {""});
    };

    return collectorContext_->allocateSharedMem(options, keys, collect_func);
  }

  // Virtual functions for applications.
  Extractor& getExtractor() override {
    return collectorContext_->getCollectors()->getExtractor();
  }

  const elf::game::Base* getGameC(int game_idx) const override {
    if (_check_game_idx(game_idx)) {
      logger_->error("Invalid game_idx [{}]", game_idx);
      return nullptr;
    }

    return games_[game_idx].get();
  }

  elf::game::Base* getGame(int game_idx) override {
    if (_check_game_idx(game_idx)) {
      logger_->error("Invalid game_idx [{}]", game_idx);
      return nullptr;
    }

    return games_[game_idx].get();
  }

  BatchContext* getBatchContext() {
    return batchContext_.get();
  }
  CollectorContext* getCollectorContext() {
    return collectorContext_.get();
  }

  ~GameContext() {
    games_.clear();
    batchContext_.reset(nullptr);
    collectorContext_.reset(nullptr);
  }

 private:
  bool _check_game_idx(int game_idx) const {
    return game_idx < 0 || game_idx >= (int)games_.size();
  }

 private:
  elf::GameClientInterface *client_;

  std::unique_ptr<BatchContext> batchContext_;
  std::unique_ptr<CollectorContext> collectorContext_;
  std::vector<std::unique_ptr<elf::game::Base>> games_;

  std::shared_ptr<spdlog::logger> logger_;
};

} // namespace elf
