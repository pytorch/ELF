/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <deque>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <sched.h>

#include <zmq.hpp>
#include "elf/logging/IndexedLoggerFactory.h"

namespace elf {

namespace distri {

inline std::string s_recv(zmq::socket_t& socket) {
  zmq::message_t message;
  socket.recv(&message);

  return std::string(static_cast<char*>(message.data()), message.size());
}

inline bool s_recv_noblock(zmq::socket_t& socket, std::string* msg) {
  zmq::message_t message;

  if (socket.recv(&message, ZMQ_NOBLOCK)) {
    *msg = std::string(static_cast<char*>(message.data()), message.size());
    return true;
  } else {
    return false;
  }
}

//  Convert string to 0MQ string and send to socket
inline bool s_send(zmq::socket_t& socket, const std::string& s) {
  zmq::message_t message(s.size());
  memcpy(message.data(), s.data(), s.size());

  bool rc = socket.send(message);
  return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
inline bool s_sendmore(zmq::socket_t& socket, const std::string& s) {
  zmq::message_t message(s.size());
  memcpy(message.data(), s.data(), s.size());

  bool rc = socket.send(message, ZMQ_SNDMORE);
  return (rc);
}

inline std::string s_version() {
  int major, minor, patch;
  zmq_version(&major, &minor, &patch);

  std::stringstream ss;
  ss << major << "." << minor << "." << patch;
  return ss.str();
}

inline void set_opts(zmq::socket_t* opt) {
  opt->setsockopt(ZMQ_LINGER, 1000);
  opt->setsockopt(ZMQ_BACKLOG, 32767);
  opt->setsockopt(ZMQ_RCVHWM, 32767);
  opt->setsockopt(ZMQ_SNDHWM, 32767);
}

class SegmentedRecv {
 public:
  SegmentedRecv(zmq::socket_t& socket)
      : socket_(socket),
        logger_(
            elf::logging::getLogger("elf::distributed::SegmentedRecv-", "")) {}

  /*
  void recvBlocked(size_t n, std::vector<std::string>* p_msgs) {
    auto& msgs = *p_msgs;
    get_from_buffer(n, p_msgs);
    while (msgs.size() < n) {
      msgs.push_back(s_recv(socket_));
      // std::cout << "Receive block "
      //         << i << "/" << n << ": " << msgs[i] << std::endl;
    }
  }
  */

  bool recvNonblocked(size_t n, std::vector<std::string>* p_msgs) {
    p_msgs->clear();
    while (p_msgs->size() < n) {
      std::string s;
      if (getNoblock(&s)) {
        p_msgs->push_back(s);
      } else {
        // If we don't get the message, return everything to last_msgs_.
        revoke(p_msgs);
        return false;
      }
    }
    return true;
  }

  bool recvNonblockedWithPrefix(
      size_t n,
      const std::string& prefix,
      size_t prefix_idx,
      std::vector<std::string>* p_msgs) {
    std::vector<std::string> msgs;
    p_msgs->clear();
    //
    while (p_msgs->size() < n) {
      std::string s;
      if (getNoblock(&s)) {
        msgs.push_back(s);
        bool at_prefix = p_msgs->size() == prefix_idx;
        if (!at_prefix || s == prefix) {
          p_msgs->push_back(s);
        } else if (at_prefix) {
          logger_->warn("recvNonblockedWithPrefix: {} != {}", prefix, s);
        }
      } else {
        // If we don't get the message, return everything to last_msgs_.
        revoke(&msgs);
        p_msgs->clear();
        return false;
      }
    }
    return true;
  }

  /*
  void recvPrefixBlocked(const std::string& prefix) {
    // std::cout << "Wait for prefix " << prefix << " blocked " << std::endl;
    std::vector<std::string> msgs;
    do {
      recvBlocked(1, &msgs);
    } while (msgs[0] != prefix);
  }
  */

 private:
  zmq::socket_t& socket_;
  std::deque<std::string> last_msgs_;
  std::shared_ptr<spdlog::logger> logger_;

  bool getNoblock(std::string* s) {
    if (!last_msgs_.empty()) {
      *s = last_msgs_.front();
      last_msgs_.pop_front();
      return true;
    } else {
      return s_recv_noblock(socket_, s);
    }
  }

  void revoke(std::vector<std::string>* msgs) {
    if (msgs->empty())
      return;

    size_t i = msgs->size();
    do {
      i--;
      last_msgs_.push_front((*msgs)[i]);
    } while (i > 0);
    msgs->clear();
  }
};

class SameThreadChecker {
 public:
  SameThreadChecker()
      : logger_(elf::logging::getLogger(
            "elf::distributed::SameThreadChecker-",
            "")) {
    id_ = std::this_thread::get_id();
  }

  bool check() const {
    auto id = std::this_thread::get_id();
    return id_ == id;
  }

  virtual ~SameThreadChecker() {
    if (!check()) {
      logger_->error(
          "Thread used to construct is different from the destructor thread!");
      assert(false);
    }
  }

 private:
  std::thread::id id_;
  std::shared_ptr<spdlog::logger> logger_;
};

static const std::string kSendPrefix = "ZMQSend";
static const std::string kRecvPrefix = "ZMQRecv";

class ZMQReceiver : public SameThreadChecker {
 public:
  ZMQReceiver(int port, bool use_ipv6)
      : context_(1),
        logger_(elf::logging::getLogger("elf::distributed::ZMQReceiver-", "")) {
    broker_.reset(new zmq::socket_t(context_, ZMQ_ROUTER));
    if (use_ipv6) {
      int ipv6 = 1;
      broker_->setsockopt(ZMQ_IPV6, &ipv6, sizeof(ipv6));
    }
    set_opts(broker_.get());

    broker_->bind("tcp://*:" + std::to_string(port));
    receiver_.reset(new SegmentedRecv(*broker_));
  }

  void send(
      const std::string& identity,
      const std::string& title,
      const std::string& msg) {
    std::lock_guard<std::mutex> locker(mutex_);

    try {
      s_sendmore(*broker_, identity);
      s_sendmore(*broker_, "");
      s_sendmore(*broker_, kRecvPrefix);
      s_sendmore(*broker_, "");
      s_sendmore(*broker_, title);
      s_sendmore(*broker_, "");

      //  Encourage workers until it's time to fire them
      s_send(*broker_, msg);
    } catch (const std::exception& e) {
      logger_->error("Exception encountered! {}", e.what());
    }
  }

  bool
  recv_noblock(std::string* identity, std::string* title, std::string* msg) {
    assert(msg != nullptr);
    std::lock_guard<std::mutex> locker(mutex_);

    try {
      std::vector<std::string> msgs;
      if (!receiver_->recvNonblockedWithPrefix(7, kSendPrefix, 2, &msgs))
        return false;

      *identity = msgs[0];
      *title = msgs[4];
      *msg = msgs[6];
      return true;
    } catch (const std::exception& e) {
      logger_->error("Exception encountered! {}", e.what());
      return false;
    }
  }

  ~ZMQReceiver() {
    std::lock_guard<std::mutex> locker(mutex_);

    receiver_.reset(nullptr);
    broker_.reset(nullptr);
  }

 private:
  zmq::context_t context_;
  std::unique_ptr<zmq::socket_t> broker_;
  std::unique_ptr<SegmentedRecv> receiver_;
  std::mutex mutex_;
  std::shared_ptr<spdlog::logger> logger_;
};

class ZMQSender : public SameThreadChecker {
 public:
  ZMQSender(
      const std::string& id,
      const std::string& addr,
      int port,
      bool use_ipv6)
      : context_(1),
        logger_(elf::logging::getLogger("elf::distributed::ZMQSender-", "")) {
    sender_.reset(new zmq::socket_t(context_, ZMQ_DEALER));
    if (use_ipv6) {
      int ipv6 = 1;
      sender_->setsockopt(ZMQ_IPV6, &ipv6, sizeof(ipv6));
    }
    sender_->setsockopt(ZMQ_IDENTITY, id.c_str(), id.length());
    set_opts(sender_.get());

    sender_->connect("tcp://" + addr + ":" + std::to_string(port));
    receiver_.reset(new SegmentedRecv(*sender_));
  }

  void send(const std::string& title, const std::string& msg) {
    std::lock_guard<std::mutex> locker(mutex_);
    try {
      s_sendmore(*sender_, "");
      s_sendmore(*sender_, kSendPrefix);
      s_sendmore(*sender_, "");
      s_sendmore(*sender_, title);
      s_sendmore(*sender_, "");
      s_send(*sender_, msg);
    } catch (const std::exception& e) {
      logger_->error("Exception encountered! {}", e.what());
    }
  }

  bool recv_noblock(std::string* title, std::string* msg) {
    assert(msg != nullptr);

    std::lock_guard<std::mutex> locker(mutex_);
    try {
      std::vector<std::string> msgs;

      if (!receiver_->recvNonblockedWithPrefix(6, kRecvPrefix, 1, &msgs))
        return false;

      *title = msgs[3];
      *msg = msgs[5];
      return true;
    } catch (const std::exception& e) {
      logger_->error("Exception encountered! {}", e.what());
      return false;
    }
  }

  ~ZMQSender() {
    std::lock_guard<std::mutex> locker(mutex_);

    receiver_.reset(nullptr);

    sender_.reset(nullptr);
  }

 private:
  zmq::context_t context_;
  std::unique_ptr<zmq::socket_t> sender_;
  std::unique_ptr<SegmentedRecv> receiver_;
  std::mutex mutex_;
  std::shared_ptr<spdlog::logger> logger_;
};

} // namespace distri

} // namespace elf
