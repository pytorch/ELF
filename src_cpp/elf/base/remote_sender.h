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

class RemoteSenderInstance {
 public:
  RemoteSenderInstance(const elf::shared::Options& netOptions, RecvFunc recv) 
    : recv_(recv) {
    assert(recv_ != nullptr);
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
      recv_(recv_msg);
      return true;
    };

    server_->setCallbacks(proc, replier);
    server_->start();
  }

 private:
  std::unique_ptr<msg::Server> server_;
  Queue<std::string> q_;
  RecvFunc recv_ = nullptr;
};

enum SendPattern { RAND_ONE, ALL };

class RemoteSenderInstances {
 public:
  RemoteSenderInstances(const elf::shared::Options &netOptions, 
      RecvFunc recv, SendPattern pattern) 
    : netOptions_(netOptions), recv_(recv), 
      rng_(time(NULL)), pattern_(pattern) {}

  int addServer(const std::string &identity) {
    json info;
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = servers_.find(identity);
    if (it == servers_.end()) {
      auto netOptions = netOptions_;
      netOptions.port += servers_.size();
      server_ids_.push_back(identity);
      auto &server = servers_[identity];
      server.reset(new RemoteSenderInstance(netOptions, recv_)); 
      server->start();

      std::cout << timestr() << ", New connection from " << identity 
        << ", assigned port: " << netOptions.port << std::endl;

      return netOptions.port;
    } else {
      return -1;
    }
  }

  void send(const std::string &msg) {
    RemoteSenderInstance *q = nullptr;

    while (server_ids_.empty()) {
      std::cout << "No server available, wait for 10s" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    // std::cout << "get a server to send ... " << std::endl;
    //
    switch (pattern_) {
      case RAND_ONE:
        {
          std::lock_guard<std::mutex> lock(mutex_);
          int idx = rng_() % server_ids_.size();
          q = servers_[server_ids_[idx]].get();
        }

        // std::cout << "sending message ... " << std::endl;
        assert(q);
        q->push(msg);
        break;
      case ALL:
        std::vector<RemoteSenderInstance *> servers;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          for (const auto &id : server_ids_) {
            servers.push_back(servers_[id].get());
          }
        }
        for (RemoteSenderInstance *sender : servers) {
          assert(sender);
          sender->push(msg);
        }
        break;
    }
  }

 private:
  const elf::shared::Options netOptions_;
  RecvFunc recv_ = nullptr;

  std::mutex mutex_;
  std::mt19937 rng_;
  SendPattern pattern_;
  std::vector<std::string> server_ids_;
  std::unordered_map<std::string, std::unique_ptr<RemoteSenderInstance>> servers_;
};

class ReplyQs {
 public:
  using Q = Queue<std::string>; 
  ReplyQs() : signature_(std::to_string(time(NULL))) {}

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
        reply_qs_[jj["idx"]]->push(jj["content"]);
      }
    }
  }

  void pop(int idx, std::string *msg) {
    reply_qs_[idx]->pop(msg);
  }

  const std::string &getSignature() { return signature_; }

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

class RemoteSender : public GameContext {
 public:
  RemoteSender(const Options& options, const msg::Options& net, SendPattern pattern)
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
    using std::placeholders::_1;
    servers_.reset(new RemoteSenderInstances(netOptions, std::bind(&ReplyQs::push, &replies_, _1), pattern));
  }

  void start() override {
    auto replier = [&](const std::string& identity, std::string* reply_msg) {
      json info;
      info["valid"] = true;
      info["signature"] = replies_.getSignature();
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

 protected:
  int addQueue() {
    return replies_.addQueue();
  }

  void sendToClient(std::string &&msg) {
    servers_->send(std::move(msg));
  }

  void getFromClient(int queue_idx, std::string *msg) {
    replies_.pop(queue_idx, msg);
  }

 private:
  std::unique_ptr<msg::Server> ctrl_server_;
  std::unique_ptr<RemoteSenderInstances> servers_;
  ReplyQs replies_;
};

} // namespace remote
} // namespace elf
