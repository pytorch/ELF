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
  struct Stats {
    std::atomic<int> client_size;
    std::atomic<int> buffer_size;
    std::atomic<int> failed_count;
    std::atomic<int> msg_count;
    std::atomic<uint64_t> total_msg_size;

    Stats()
        : client_size(0),
          buffer_size(0),
          failed_count(0),
          msg_count(0),
          total_msg_size(0) {}

    std::string info() const {
      std::stringstream ss;
      ss << "#msg: " << buffer_size << " #client: " << client_size << ", ";
      ss << "Msg count: " << msg_count
         << ", avg msg size: " << (float)(total_msg_size) / msg_count
         << ", failed count: " << failed_count;
      return ss.str();
    }

    void feed(const RQInterface::InsertInfo& insert_info) {
      if (!insert_info.success) {
        failed_count++;
      } else {
        buffer_size += insert_info.delta;
        msg_count++;
        total_msg_size += insert_info.msg_size;
      }
    }
  };

  using ReplyFunc =
      std::function<bool(Reader*, const std::string&, std::string*)>;
  using StartFunc = std::function<void()>;

  Reader(const std::string& filename, const Options& opt)
      : receiver_(opt.port, opt.use_ipv6),
        options_(opt),
        db_name_(filename),
        rng_(time(NULL)),
        done_(false) {}

  void startReceiving(
      RQInterface* rq,
      StartFunc start_func = nullptr,
      ReplyFunc replier = nullptr) {
    receiver_thread_.reset(new std::thread(
        [=](Reader* reader) {
          if (start_func != nullptr)
            start_func();
          reader->threaded_receive_msg(rq, replier);
        },
        this));
  }

  const Stats& stats() const {
    return stats_;
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
  Stats stats_;

  std::atomic_bool done_;

  void threaded_receive_msg(RQInterface* rq, ReplyFunc replier = nullptr) {
    std::string identity, title, msg;
    while (!done_.load()) {
      if (!receiver_.recv_noblock(&identity, &title, &msg)) {
        std::cout << elf_utils::now()
                  << ", Reader: no message, wait for 10 sec ... " << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        continue;
      }

      if (title == "ctrl") {
        stats_.client_size++;
        std::cout << elf_utils::now() << " Ctrl from " << identity << "["
                  << stats_.client_size << "]: " << msg << std::endl;
        // receiver_.send(identity, "ctrl", "");
      } else if (title == "content") {
        // Save to
        auto insert_info = rq->Insert(msg, &rng_);
        stats_.feed(insert_info);
        if (insert_info.success) {
          if (options_.verbose) {
            std::cout << "Content from " << identity
                      << ", msg_size: " << msg.size() << ", " << stats_.info()
                      << std::endl;
          }
          if (stats_.msg_count % 1000 == 0) {
            std::cout << elf_utils::now() << ", last_identity: " << identity
                      << ", " << stats_.info() << std::endl;
          }
        } else {
          std::cout << "Msg insertion error! from " << identity << std::endl;
        }
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
