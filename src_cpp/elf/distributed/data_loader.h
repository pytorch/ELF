/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "shared_reader.h"
#include "shared_rw_buffer3.h"

namespace elf {
namespace msg { 

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

  void feed(const elf::shared::InsertInfo& insert_info) {
    if (!insert_info.success) {
      failed_count++;
    } else {
      buffer_size += insert_info.delta;
      msg_count++;
      total_msg_size += insert_info.msg_size;
    }
  }
};

class DataInterface {
 public:
  virtual void OnStart() {}
  virtual elf::shared::InsertInfo OnReceive(
      const std::string& identity,
      const std::string& msg) = 0;
  virtual bool OnReply(const std::string& identity, std::string* msg) = 0;
};

class DataOnlineLoader {
 public:
  DataOnlineLoader(const elf::shared::Options& net_options)
      : logger_(elf::logging::getLogger("DataOnlineLoader-", "")) {
    server_.reset(new elf::msg::Server(net_options));
    std::cout << server_->info() << std::endl;
  }

  void start(DataInterface* interface) {
    auto proc_func = [&, interface](
                         const std::string& identity,
                         const std::string& msg) -> bool {
      try {
        auto info = interface->OnReceive(identity, msg);
        stats_.feed(info);
        /*
        if (options_.verbose) {
          std::cout << "Content from " << identity
            << ", msg_size: " << msg.size() << ", " << stats_.info()
            << std::endl;
        }
        */
        if (stats_.msg_count % 1000 == 0) {
          std::cout << elf_utils::now() << ", last_identity: " << identity
                    << ", " << stats_.info() << std::endl;
        }
        return info.success;
      } catch (...) {
        logger_->error("Data malformed! String is {}", msg);
        return false;
      }
    };

    auto replier_func =
        [&, interface](const std::string& identity, std::string* msg) -> bool {
      interface->OnReply(identity, msg);

      if (logger_->should_log(spdlog::level::level_enum::debug)) {
        logger_->debug(
            "Replier: about to send: recipient {}; msg {}; reader {}",
            identity,
            *msg,
            server_->info());
      }
      return true;
    };

    server_->setCallbacks(proc_func, replier_func);
    server_->start([interface]() { interface->OnStart(); });
  }

  ~DataOnlineLoader() {}

 private:
  std::unique_ptr<elf::msg::Server> server_;
  Stats stats_;

  std::shared_ptr<spdlog::logger> logger_;
};

} // namespace msg
} // namespace elf
