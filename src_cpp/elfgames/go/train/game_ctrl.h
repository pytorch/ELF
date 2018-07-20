/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "data_loader.h"

#include "ctrl_eval.h"
#include "ctrl_selfplay.h"
#include "elf/base/context.h"
#include "elf/base/dispatcher.h"
#include "elf/concurrency/ConcurrentQueue.h"
#include "elf/concurrency/Counter.h"

#include "../common/game_stats.h"
#include "../common/go_game_specific.h"
#include "../common/go_state_ext.h"
#include "../common/notifier.h"

using namespace std::chrono_literals;
using ReplayBuffer = elf::shared::ReaderQueuesT<Record>;
using ThreadedCtrlBase = elf::ThreadedCtrlBase;
using Ctrl = elf::Ctrl;
using Addr = elf::Addr;

class ThreadedCtrl : public ThreadedCtrlBase {
 public:
  ThreadedCtrl(
      Ctrl& ctrl,
      elf::GameClient* client,
      ReplayBuffer* replay_buffer,
      const GameOptions& options,
      const elf::ai::tree_search::TSOptions& mcts_opt)
      : ThreadedCtrlBase(ctrl, 10000),
        replay_buffer_(replay_buffer),
        options_(options),
        client_(client),
        rng_(time(NULL)) {
    selfplay_.reset(new SelfPlaySubCtrl(options, mcts_opt));
    eval_.reset(new EvalSubCtrl(options, mcts_opt));
    // std::cout << "Thread id: " << std::this_thread::get_id() << std::endl;

    ctrl_.reg();
    ctrl_.addMailbox<_ModelUpdateStatus>();
  }

  void Start() {
    if (!ctrl_.isRegistered()) {
      // std::cout << "Start(): id: " << std::this_thread::get_id() << ", not
      // registered!" << std::endl;
      ctrl_.reg();
    } else {
      // std::cout << "Start(): id: " << std::this_thread::get_id() <<", already
      // registered!" << std::endl;
    }
    ctrl_.addMailbox<_ModelUpdateStatus>();
    start<std::pair<Addr, int64_t>>();
  }

  void waitForSufficientSelfplay(int64_t selfplay_ver) {
    SelfPlaySubCtrl::CtrlResult res;
    while ((res = selfplay_->needWaitForMoreSample(selfplay_ver)) ==
           SelfPlaySubCtrl::CtrlResult::INSUFFICIENT_SAMPLE) {
      std::cout << elf_utils::now() << ", Insufficient sample for model "
                << selfplay_ver << "... waiting 30s" << std::endl;
      std::this_thread::sleep_for(30s);
    }

    if (res == SelfPlaySubCtrl::CtrlResult::SUFFICIENT_SAMPLE) {
      std::cout << elf_utils::now() << ", Sufficient sample for model "
                << selfplay_ver << std::endl;
      selfplay_->notifyCurrentWeightUpdate();
    }
  }

  void updateModel(int64_t new_model) {
    sendToThread(std::make_pair(ctrl_.getAddr(), new_model));
    _ModelUpdateStatus dummy;
    ctrl_.waitMail(&dummy);
  }

  bool checkNewModel(ClientManager* manager) {
    int64_t new_model = eval_->updateState(*manager);

    // If there is at least one true eval.
    if (new_model >= 0) {
      updateModel(new_model);
      return true;
    }

    return false;
  }

  bool setInitialVersion(int64_t init_version) {
    std::cout << "Setting init version: " << init_version << std::endl;
    eval_->setBaselineModel(init_version);

    if (selfplay_->getCurrModel() < 0) {
      selfplay_->setCurrModel(eval_->getBestModel());
    }
    return true;
  }

  void addNewModelForEvaluation(int64_t selfplay_ver, int64_t new_version) {
    if (options_.eval_num_games == 0) {
      // And send a message to start the process.
      updateModel(new_version);
    } else {
      eval_->addNewModelForEvaluation(selfplay_ver, new_version);
      // For offline training, we don't need to wait..
      if (options_.mode != "offline_train") {
        waitForSufficientSelfplay(selfplay_ver);
      }
    }
  }

  void setEvalMode(int64_t new_ver, int64_t old_ver) {
    eval_->setBaselineModel(old_ver);
    eval_->addNewModelForEvaluation(old_ver, new_ver);
    eval_mode_ = true;
  }

  // Call by writer thread.
  std::vector<FeedResult> onSelfplayGames(const std::vector<Record>& records) {
    // Receive selfplay/evaluation games.
    std::vector<FeedResult> res(records.size());

    for (size_t i = 0; i < records.size(); ++i) {
      res[i] = selfplay_->feed(records[i]);
    }

    return res;
  }

  std::vector<FeedResult> onEvalGames(
      const ClientInfo& info,
      const std::vector<Record>& records) {
    // Receive selfplay/evaluation games.
    std::vector<FeedResult> res(records.size());

    for (size_t i = 0; i < records.size(); ++i) {
      res[i] = eval_->feed(info, records[i]);
    }

    return res;
  }

  void fillInRequest(const ClientInfo& info, MsgRequest* request) {
    request->vers.set_wait();
    request->client_ctrl.client_type = info.type();

    switch (info.type()) {
      case CLIENT_SELFPLAY_ONLY:
        if (!eval_mode_) {
          selfplay_->fillInRequest(info, request);
        }
        break;
      case CLIENT_EVAL_THEN_SELFPLAY:
        eval_->fillInRequest(info, request);
        if (request->vers.wait() && !eval_mode_) {
          selfplay_->fillInRequest(info, request);
        }
        break;
      case CLIENT_INVALID:
        std::cout << "Warning! Invalid client_type! " << std::endl;
        break;
    }
  }

 protected:
  enum _ModelUpdateStatus { MODEL_UPDATED };

  ReplayBuffer* replay_buffer_ = nullptr;
  std::unique_ptr<SelfPlaySubCtrl> selfplay_;
  std::unique_ptr<EvalSubCtrl> eval_;

  bool eval_mode_ = false;

  const GameOptions options_;
  elf::GameClient* client_ = nullptr;
  std::mt19937 rng_;

  std::string kTrainCtrl = "train_ctrl";

  void on_thread() override {
    std::pair<Addr, int64_t> data;
    if (!ctrl_.peekMail(&data, 0))
      return;

    int64_t ver = data.second;

    eval_->setBaselineModel(ver);
    int64_t old_ver = selfplay_->getCurrModel();
    selfplay_->setCurrModel(ver);

    // After setCurrModel, new model from python side with the old selfplay_ver
    // will not enter the replay buffer
    std::cout << "Updating .. old_ver: " << old_ver << ", new_ver: " << ver
              << std::endl;
    // A better model is found, clean up old games (or not?)
    if (!options_.keep_prev_selfplay) {
      replay_buffer_->clear();
    }

    // Data now prepared ready,
    // Send message to deblock the caller.
    ctrl_.sendMail(data.first, MODEL_UPDATED);

    // Then notify the python side for the new selfplay version.
    // Then we send information to Python side.
    MsgVersion msg;
    msg.model_ver = ver;
    elf::FuncsWithState funcs =
        client_->BindStateToFunctions({kTrainCtrl}, &msg);
    client_->sendWait({kTrainCtrl}, &funcs);
  }
};

class TrainCtrl : public DataInterface {
 public:
  TrainCtrl(
      Ctrl& ctrl,
      int num_games,
      elf::GameClient* client,
      const GameOptions& options,
      const elf::ai::tree_search::TSOptions& mcts_opt)
      : ctrl_(ctrl),
        rng_(time(NULL)),
        selfplay_record_("tc_selfplay"),
        logger_(elf::logging::getLogger("TrainCtrl-", "")) {
    // Register sender for python thread.
    elf::shared::RQCtrl rq_ctrl;
    rq_ctrl.num_reader = options.num_reader;
    rq_ctrl.ctrl.queue_min_size = options.q_min_size;
    rq_ctrl.ctrl.queue_max_size = options.q_max_size;

    replay_buffer_.reset(new ReplayBuffer(rq_ctrl));
    logger_->info(
        "Finished initializing replay_buffer {}", replay_buffer_->info());
    threaded_ctrl_.reset(new ThreadedCtrl(
        ctrl_, client, replay_buffer_.get(), options, mcts_opt));
    client_mgr_.reset(new ClientManager(
        num_games,
        options.client_max_delay_sec,
        options.expected_num_clients,
        0.5));
  }

  void OnStart() override {
    // Call by shared_rw thread or any thread that will call OnReceive.
    ctrl_.reg("train_ctrl");
    ctrl_.addMailbox<int>();
    threaded_ctrl_->Start();
  }

  ReplayBuffer* getReplayBuffer() {
    return replay_buffer_.get();
  }
  ThreadedCtrl* getThreadedCtrl() {
    return threaded_ctrl_.get();
  }

  bool setEvalMode(int64_t new_ver, int64_t old_ver) {
    std::cout << "Setting eval mode: new: " << new_ver << ", old: " << old_ver
              << std::endl;
    client_mgr_->setSelfplayOnlyRatio(0.0);
    threaded_ctrl_->setEvalMode(new_ver, old_ver);
    return true;
  }

  elf::shared::InsertInfo OnReceive(const std::string&, const std::string& s)
      override {
    Records rs = Records::createFromJsonString(s);
    const ClientInfo& info = client_mgr_->updateStates(rs.identity, rs.states);

    if (rs.identity.size() == 0) {
      // No identity -> offline data.
      for (auto& r : rs.records) {
        r.offline = true;
      }
    }

    std::vector<FeedResult> selfplay_res =
        threaded_ctrl_->onSelfplayGames(rs.records);

    elf::shared::InsertInfo insert_info;
    for (size_t i = 0; i < rs.records.size(); ++i) {
      if (selfplay_res[i] == FeedResult::FEEDED ||
          selfplay_res[i] == FeedResult::VERSION_MISMATCH) {
        const Record& r = rs.records[i];

        bool black_win = r.result.reward > 0;
        insert_info +=
            replay_buffer_->InsertWithParity(Record(r), &rng_, black_win);
        selfplay_record_.feed(r);
        selfplay_record_.saveAndClean(1000);
      }
    }

    std::vector<FeedResult> eval_res =
        threaded_ctrl_->onEvalGames(info, rs.records);
    threaded_ctrl_->checkNewModel(client_mgr_.get());

    recv_count_++;
    if (recv_count_ % 1000 == 0) {
      int valid_selfplay = 0, valid_eval = 0;
      for (size_t i = 0; i < rs.records.size(); ++i) {
        if (selfplay_res[i] == FeedResult::FEEDED)
          valid_selfplay++;
        if (eval_res[i] == FeedResult::FEEDED)
          valid_eval++;
      }

      std::cout << "TrainCtrl: Receive data[" << recv_count_ << "] from "
                << rs.identity << ", #state_update: " << rs.states.size()
                << ", #records: " << rs.records.size()
                << ", #valid_selfplay: " << valid_selfplay
                << ", #valid_eval: " << valid_eval << std::endl;
    }
    return insert_info;
  }

  bool OnReply(const std::string& identity, std::string* msg) override {
    ClientInfo& info = client_mgr_->getClient(identity);

    if (info.justAllocated()) {
      std::cout << "New allocated: " << identity << ", " << client_mgr_->info()
                << std::endl;
    }

    MsgRequestSeq request;
    threaded_ctrl_->fillInRequest(info, &request.request);
    request.seq = info.seq();
    *msg = request.dumpJsonString();
    info.incSeq();
    return true;
  }

 private:
  Ctrl& ctrl_;

  std::unique_ptr<ReplayBuffer> replay_buffer_;
  std::unique_ptr<ClientManager> client_mgr_;
  std::unique_ptr<ThreadedCtrl> threaded_ctrl_;

  int recv_count_ = 0;
  std::mt19937 rng_;

  // SelfCtrl has its own record buffer to save EVERY game it has received.
  RecordBufferSimple selfplay_record_;

  std::shared_ptr<spdlog::logger> logger_;
};
