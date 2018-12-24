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

#include "elf/distributed/data_loader.h"

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

constexpr int CLIENT_SELFPLAY_ONLY = 0;
constexpr int CLIENT_EVAL_THEN_SELFPLAY = 1;

class ThreadedCtrl : public ThreadedCtrlBase {
 public:
  ThreadedCtrl(
      Ctrl& ctrl,
      elf::GameClientInterface* client,
      ReplayBuffer* replay_buffer,
      const GameOptionsTrain& options)
      : ThreadedCtrlBase(ctrl, 10000),
        replay_buffer_(replay_buffer),
        options_(options),
        client_(client),
        rng_(time(NULL)) {
    selfplay_.reset(new SelfPlaySubCtrl(options));
    eval_.reset(new EvalSubCtrl(options));
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
      if (options_.common.mode != "offline_train") {
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

  const GameOptionsTrain options_;
  elf::GameClientInterface* client_ = nullptr;
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
    client_->sendWait(kTrainCtrl, &msg);
  }
};

