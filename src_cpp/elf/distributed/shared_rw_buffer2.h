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
  std::string identity;

  std::string info() const {
    std::stringstream ss;
    ss << "[" << identity << "] ";
    if (addr == "") {
      ss << "Listen@" << port;
    } else {
      ss << "Connect to " << addr << ":" << port;
    }
    ss << ", ipv6: " << elf_utils::print_bool(use_ipv6)
       << ", verbose: " << elf_utils::print_bool(verbose);
    return ss.str();
  }
};

class Writer {
 public:
  // Constructor.
  Writer(const Options& opt) : rng_(time(NULL)), options_(opt) {
    identity_ = options_.identity + "-" + get_id(rng_);
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

class Reader {
 public:
  using ProcessFunc = std::function<
      bool(Reader*, const std::string& identity, const std::string& recv_msg)>;

  using ReplyFunc = std::function<
      bool(Reader*, const std::string& identity, std::string* reply_msg)>;

  using StartFunc = std::function<void()>;

  Reader(const std::string& filename, const Options& opt)
      : receiver_(opt.port, opt.use_ipv6),
        options_(opt),
        db_name_(filename),
        rng_(time(NULL)),
        done_(false) {}

  void startReceiving(
      ProcessFunc proc_func,
      ReplyFunc replier = nullptr,
      StartFunc start_func = nullptr) {
    receiver_thread_.reset(new std::thread(
        [=](Reader* reader) {
          if (start_func != nullptr)
            start_func();
          reader->threaded_receive_msg(proc_func, replier);
        },
        this));
  }

  std::string info() const {
    std::stringstream ss;
    ss << "ZMQVer: " << elf::distri::s_version() << " Reader[db=" << db_name_
       << "] " << options_.info();
    return ss.str();
  }

  ~Reader() {
    std::cout << "Destroying Reader ... " << std::endl;
    done_ = true;
    receiver_thread_->join();

    std::cout << "Reader destroyed... " << std::endl;
  }

 private:
  elf::distri::ZMQReceiver receiver_;
  std::unique_ptr<std::thread> receiver_thread_;
  Options options_;
  std::string db_name_;
  std::mt19937 rng_;

  std::atomic_bool done_;
  int client_size_ = 0;
  int num_package_ = 0, num_failed_ = 0, num_skipped_ = 0;

  void threaded_receive_msg(
      ProcessFunc proc_func,
      ReplyFunc replier = nullptr) {
    std::string identity, title, msg;
    while (!done_.load()) {
      if (!receiver_.recv_noblock(&identity, &title, &msg)) {
        std::cout << elf_utils::now()
                  << ", Reader: no message, Stats: " << num_package_ << "/"
                  << num_failed_ << "/" << num_skipped_
                  << ", wait for 10 sec ... " << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        continue;
      }

      if (title == "ctrl") {
        client_size_++;
        std::cout << elf_utils::now() << " Ctrl from " << identity << "["
                  << client_size_ << "]: " << msg << std::endl;
        // receiver_.send(identity, "ctrl", "");
      } else if (title == "content") {
        if (!proc_func(this, identity, msg)) {
          std::cout << "Msg processing error! from " << identity << std::endl;
          num_failed_++;
        } else {
          num_package_++;
        }
      } else {
        std::cout << elf_utils::now() << " Skipping unknown title: \"" << title
                  << "\", identity: \"" << identity << "\"" << std::endl;
        num_skipped_++;
      }

      // Send reply if there is any.
      if (replier != nullptr) {
        std::string reply;
        if (replier(this, identity, &reply)) {
          receiver_.send(identity, "reply", reply);
        }
      }
    }
  }
};

} // namespace shared

} // namespace elf
