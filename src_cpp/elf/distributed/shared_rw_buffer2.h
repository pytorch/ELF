/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#include "elf/utils/utils.h"

#include "shared_reader.h"
#include "zmq_util.h"

namespace elf {

namespace shared {

struct Options {
  std::string addr;
  int port = 5556;
  bool use_ipv6 = true;
  bool verbose = false;
  int64_t usec_resend_when_no_msg = -1;
  // 10s
  int64_t usec_sleep_when_no_msg = 10000000;
  std::string identity;

  bool no_prefix_on_identity = false;
  // hello message from client to server, default is "".
  std::string hello_message;

  std::string info() const {
    std::stringstream ss;
    ss << "[" << identity << "] ";
    if (addr == "") {
      ss << "Listen@" << port;
    } else {
      ss << "Connect to " << addr << ":" << port;
    }
    ss << "usec_sleep_when_no_msg: " << usec_sleep_when_no_msg
       << " usec, usec_resend_when_no_msg: " << usec_resend_when_no_msg
       << ", ipv6: " << elf_utils::print_bool(use_ipv6)
       << ", verbose: " << elf_utils::print_bool(verbose);
    return ss.str();
  }
};

class Writer {
 public:
  // Constructor.
  Writer(const Options& opt) 
    : rng_(time(NULL)), options_(opt) {
    identity_ = options_.identity;
    if (! opt.no_prefix_on_identity) {
      identity_ += "-" + std::to_string(options_.port) + "-" + get_id(rng_);
    }
    sender_.reset(new elf::distri::ZMQSender(
        identity_, options_.addr, options_.port, options_.use_ipv6));
  }

  const std::string& identity() const {
    return identity_;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "ZMQVer: " << elf::distri::s_version() << " Writer[" << identity_
       << "]. " << options_.info();
    return ss.str();
  }

  bool Insert(const std::string& s) {
    write_mutex_.lock();
    sender_->send("content", s);
    write_mutex_.unlock();
    return true;
  }

  bool Ctrl(const std::string& msg) {
    sender_->send("ctrl", msg);
    return true;
  }

  bool getReplyNoblock(std::string* msg) {
    std::string title;
    bool received = sender_->recv_noblock(&title, msg);
    if (!received)
      return false;

    if (title != "reply") {
      std::cout << "Writer[" << identity_ << "] wrong title " << title
                << " in getReplyNoblock()" << std::endl;
      return false;
    } else {
      return true;
    }
  }

  ~Writer() {
    // std::cout << "Started writer distructor" << std::endl;
    sender_.reset(nullptr);
    // std::cout << "Writer distructor done" << std::endl;
  }

 private:
  std::unique_ptr<elf::distri::ZMQSender> sender_;
  std::mt19937 rng_;
  std::string identity_;
  Options options_;
  std::mutex write_mutex_;

  static std::string get_id(std::mt19937& rng) {
    long host_name_max = sysconf(_SC_HOST_NAME_MAX);
    if (host_name_max <= 0)
      host_name_max = _POSIX_HOST_NAME_MAX;

    char* hostname = new char[host_name_max + 1];
    gethostname(hostname, host_name_max);

    std::stringstream ss;
    ss << hostname;
    ss << std::hex;
    for (int i = 0; i < 4; ++i) {
      ss << "-";
      ss << (rng() & 0xffff);
    }
    delete[] hostname;
    return ss.str();
  }
};

} // namespace shared

} // namespace elf
