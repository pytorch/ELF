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
  // std::cout << "Sending " << s << std::endl;
  return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
inline bool s_sendmore(zmq::socket_t& socket, const std::string& s) {
  zmq::message_t message(s.size());
  memcpy(message.data(), s.data(), s.size());

  bool rc = socket.send(message, ZMQ_SNDMORE);
  // std::cout << "Sending more " << s << std::endl;
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
  SegmentedRecv(zmq::socket_t& socket) : socket_(socket) {}

  void recvBlocked(int n, std::vector<std::string>* p_msgs) {
    auto& msgs = *p_msgs;
    int i = get_last_msgs(n, p_msgs);
    while (i < n) {
      msgs[i] = s_recv(socket_);
      // std::cout << "Receive block "
      //         << i << "/" << n << ": " << msgs[i] << std::endl;
      i++;
    }
  }

  bool recvNonblocked(int n, std::vector<std::string>* p_msgs) {
    auto& msgs = *p_msgs;
    int i = get_last_msgs(n, p_msgs);
    //
    while (i < n) {
      if (!s_recv_noblock(socket_, &msgs[i])) {
        // If we don't get the message, return everything to last_msgs_.
        for (int j = 0; j < i; ++j) {
          last_msgs_.push_back(msgs[j]);
        }
        p_msgs->clear();
        return false;
      }
      i++;
    }
    // std::cout << "Receive noblock: " << msgs[i] << std::endl;
    return true;
  }

  void recvPrefixBlocked(const std::string& prefix) {
    // std::cout << "Wait for prefix " << prefix << " blocked " << std::endl;
    std::vector<std::string> msgs;
    do {
      recvBlocked(1, &msgs);
    } while (msgs[0] != prefix);
  }

  bool recvPrefixNonblocked(const std::string& prefix) {
    // std::cout << "Wait for prefix " << prefix << " nonblocked " << std::endl;
    std::vector<std::string> msgs;
    do {
      if (!recvNonblocked(1, &msgs))
        return false;
    } while (msgs[0] != prefix);
    return true;
  }

 private:
  zmq::socket_t& socket_;
  std::deque<std::string> last_msgs_;

  int get_last_msgs(int n, std::vector<std::string>* p_msgs) {
    auto& msgs = *p_msgs;
    msgs.resize(n);
    // if we have last_msgs_, retrieve that first.
    int i = 0;
    while (i < n && !last_msgs_.empty()) {
      msgs[i] = last_msgs_.front();
      last_msgs_.pop_front();
      i++;
    }
    return i;
  }
};

class SameThreadChecker {
 public:
  SameThreadChecker() {
    id_ = std::this_thread::get_id();
  }

  bool check() const {
    auto id = std::this_thread::get_id();
    return id_ == id;
  }

  virtual ~SameThreadChecker() {
    if (!check()) {
      std::cout << "Thread used to "
                << "construct is different from "
                << "the destructor thread !" << std::endl;
      assert(false);
    }
  }

 private:
  std::thread::id id_;
};

class ZMQReceiver : public SameThreadChecker {
 public:
  ZMQReceiver(int port, bool use_ipv6) : context_(1) {
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
      s_sendmore(*broker_, title);
      s_sendmore(*broker_, "");

      //  Encourage workers until it's time to fire them
      s_send(*broker_, msg);
    } catch (const std::exception& e) {
      std::cout << "Exception encountered! " << e.what() << std::endl;
    }
  }

  bool
  recv_noblock(std::string* identity, std::string* title, std::string* msg) {
    assert(msg != nullptr);
    std::lock_guard<std::mutex> locker(mutex_);

    try {
      std::vector<std::string> msgs;
      if (!receiver_->recvNonblocked(5, &msgs))
        return false;

      *identity = msgs[0];
      *title = msgs[2];
      *msg = msgs[4];
      return true;
    } catch (const std::exception& e) {
      std::cout << "Exception encountered! " << e.what() << std::endl;
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
};

class ZMQSender : public SameThreadChecker {
 public:
  ZMQSender(
      const std::string& id,
      const std::string& addr,
      int port,
      bool use_ipv6)
      : context_(1) {
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
      s_sendmore(*sender_, title);
      s_sendmore(*sender_, "");
      s_send(*sender_, msg);
    } catch (const std::exception& e) {
      std::cout << "Exception encountered! " << e.what() << std::endl;
    }
  }

  bool recv_noblock(std::string* title, std::string* msg) {
    assert(msg != nullptr);

    std::lock_guard<std::mutex> locker(mutex_);
    try {
      std::vector<std::string> msgs;

      if (!receiver_->recvNonblocked(4, &msgs))
        return false;

      *title = msgs[1];
      *msg = msgs[3];
      return true;
    } catch (const std::exception& e) {
      std::cout << "Exception encountered! " << e.what() << std::endl;
      return false;
    }
  }

  ~ZMQSender() {
    std::lock_guard<std::mutex> locker(mutex_);

    // std::cout << "Deleting receiver " << std::endl;
    receiver_.reset(nullptr);

    // std::cout << "Deleting sender... " << std::endl;
    sender_.reset(nullptr);

    // std::cout << "Deleted in ZMQSender " << std::endl;
  }

 private:
  zmq::context_t context_;
  std::unique_ptr<zmq::socket_t> sender_;
  std::unique_ptr<SegmentedRecv> receiver_;
  std::mutex mutex_;
};

} // namespace distri

} // namespace elf
