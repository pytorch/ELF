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
  _Server(const elf::shared::Options& netOptions, MsgQ &send_q, MsgQ &recv_q) 
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
  MsgQ &send_q_;
  MsgQ &recv_q_;
};

class Servers : public Interface {
 public:
  Servers(const elf::shared::Options &netOptions, const std::vector<std::string> &labels) 
    : netOptions_(netOptions), 
      signature_(std::to_string(std::this_thread::get_id()) + "-" + std::to_string(time(NULL))), 
      labels_(labels) {
    std::sort(labels_.begin(), labels_.end());

    netOptions_.usec_sleep_when_no_msg = 10;
    // Not used.
    netOptions_.usec_resend_when_no_msg = 10000;
    netOptions_.verbose = false;

    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    // netOptions.verbose = true;
    ctrl_server_.reset(new msg::Server(netOptions_));
    netOptions_.port ++;

    // Setup ctrl_server_
    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      if (ctrl_identities_.find(identity) != ctrl_identities_.end()) return false;

      json info;
      info["valid"] = true;
      info["server_identity"] = signature_;
      info["labels"] = labels_;
      for (int i = 0; i < kPortPerClient; ++i) {
        info["port"].push_back(netOptions_.port + i); 
      }

      *reply_msg = info.dump();
      return true;
    };

    auto proc = [&](const std::string& identity, const std::string& recv_msg) {
      if (recv_msg.empty()) return true;
      if (ctrl_identities_.find(identity) != ctrl_identities_.end()) return true;

      json j = json::parse(recv_msg);

      // Final labels. 
      std::vector<std::string> final_labels = j["labels"]; 

      auto netOptions = netOptions_;
      for (int i = 0; i < kPortPerClient; ++i) {
        send_q_.addQ(j["client_identity"][i], final_labels);
        recv_q_.addQ(j["client_identity"][i], final_labels);
        netOptions.port = j["port"][i];
        servers_.emplace_back(new _Server(netOptions, send_q_, recv_q_));
      }
      ctrl_identities_.insert(identity);

      return true;
    };

    ctrl_server_->setCallbacks(proc, replier);
    ctrl_server_->start();
  }

 private:
  const elf::shared::Options netOptions_;
  const std::string signature_;

  std::unique_ptr<msg::Server> ctrl_server_;
  std::vector<std::unique_ptr<_Server>> servers_;

  std::set<std::string> ctrl_identities_;
};

} // namespace remote
} // namespace elf
