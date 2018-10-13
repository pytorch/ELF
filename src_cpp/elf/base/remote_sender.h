/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "elf/distributed/addrs.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "remote_common.h"
#include "game_context.h"
#include "game_interface.h"

namespace elf {
namespace remote {

class _Server {
 public:
  _Server(const elf::shared::Options& netOptions, SendQ &send_q, RecvQ &recv_q) 
    : send_q_(send_q), recv_q_(recv_q) { 
    server_.reset(new msg::Server(netOptions));

    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      (void)identity;
      // Send request
      // std::cout << "Wait message .." << std::endl;
      *reply_msg = send_q_[identity].dumpClear();
      return true;
    };

    auto proc = [&](const std::string& identity, const std::string& recv_msg) {
      recv_q_[identity].parseAdd(recv_msg);
      return true;
    };

    server_->setCallbacks(proc, replier);
    server_->start();
  }

 private:
  std::unique_ptr<msg::Server> server_;
  SendQ &send_q_;
  RecvQ &recv_q_;
};

class Servers : public Interface {
 public:
  using Ls = std::vector<std::string>;

  Servers(const elf::shared::Options &netOptions, const std::vector<std::string> &labels) 
    : netOptions_(netOptions), rng_(time(NULL)), labels_(labels) {
    std::sort(labels_.begin(), labels_.end());

    netOptions_.usec_sleep_when_no_msg = 10;
    // Not used.
    netOptions_.usec_resend_when_no_msg = 10000;
    netOptions_.verbose = false;

    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    // netOptions.verbose = true;
    ctrl_server_.reset(new msg::Server(netOptions_));
    const int base_port = netOptions_.port + 1;
    for (int i = 0; i < kPortPerServer; ++i) {
      netOptions_.port = base_port + i;
      servers_.emplace_back(new _Server(netOptions_, send_q_, recv_q_));
    }
    netOptions_.port = base_port; 

    auto controller = [&](const std::string& identity, const std::string& msg) {
      json j = json::parse(msg);
      // std::cout << j << std::endl;
      std::vector<std::string> labels = getIntersect(labels_, j["labels"]);

      // Final labels. 
      std::lock_guard<std::mutex> lock(mutex_);
      identities_[identity].labels = labels;
      return true;
    };

    // Setup ctrl_server_
    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      std::vector<std::string> labels;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto &client = identities_[identity]; 
        if (! client.new_client) return false;
        labels = client.labels;
      }

      json info;
      info["valid"] = true;
      info["labels"] = labels;
      int start_port = rng_() % kPortPerServer;
      for (int i = 0; i < kPortPerClient; ++i) {
        int curr_port = netOptions_.port + start_port; 

        const std::string id = identity + "_" + std::to_string(curr_port) + "_" + std::to_string(rng_() % 10000);
        send_q_.addQ(id, labels);
        recv_q_.addQ(id, labels);

        info["client_identity"].push_back(id);
        info["port"].push_back(curr_port); 
        start_port = (start_port + 1) % kPortPerServer;
      }

      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &client = identities_[identity]; 
        client.new_client = false;
      }

      *reply_msg = info.dump();
      return true;
    };

    auto proc = [&](const std::string& identity, const std::string& recv_msg) {
      (void)identity;
      (void)recv_msg;
      return true;
    };

    ctrl_server_->setCallbacks(proc, replier, controller);
    ctrl_server_->start();
  }

 private:
  shared::Options netOptions_;

  std::mt19937 rng_;

  std::unique_ptr<msg::Server> ctrl_server_;
  std::vector<std::unique_ptr<_Server>> servers_;

  std::vector<std::string> labels_;

  struct IdentityInfo {
    bool new_client = true;
    std::vector<std::string> labels;
  };

  std::mutex mutex_;
  std::unordered_map<std::string, IdentityInfo> identities_;

  static std::vector<std::string> getIntersect(
        const std::vector<std::string> &labels1, 
        const std::vector<std::string> &labels2) {
      // Compute an intersection of j["labels"] and our labels. 
      std::unordered_map<std::string, int> tmp_counts;
      for (const auto &label : labels1) tmp_counts[label] ++;
      for (const auto &label : labels2) tmp_counts[label] ++;

      std::vector<std::string> final_labels;
      for (const auto &p : tmp_counts) 
        if (p.second == 2) 
          final_labels.push_back(p.first);
      return final_labels;
  }
};

} // namespace remote
} // namespace elf
