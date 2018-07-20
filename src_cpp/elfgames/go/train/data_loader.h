/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "../common/record.h"
#include "elf/distributed/shared_reader.h"
#include "elf/distributed/shared_rw_buffer2.h"

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
    auto curr_timestamp = time(NULL);
    const std::string database_name =
        "data-" + std::to_string(curr_timestamp) + ".db";
    reader_.reset(new elf::shared::Reader(database_name, net_options));
    std::cout << reader_->info() << std::endl;
  }

  void start(DataInterface* interface) {
    auto proc_func = [&, interface](
                         elf::shared::Reader* reader,
                         const std::string& identity,
                         const std::string& msg) -> bool {
      (void)reader;

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

    auto replier_func = [&, interface](
                            elf::shared::Reader* reader,
                            const std::string& identity,
                            std::string* msg) -> bool {
      (void)reader;

      interface->OnReply(identity, msg);

      if (logger_->should_log(spdlog::level::level_enum::debug)) {
        logger_->debug(
            "Replier: about to send: recipient {}; msg {}; reader {}",
            identity,
            *msg,
            reader_->info());
      }
      return true;
    };

    reader_->startReceiving(
        proc_func, replier_func, [interface]() { interface->OnStart(); });
  }

  ~DataOnlineLoader() {}

 private:
  std::unique_ptr<elf::shared::Reader> reader_;
  Stats stats_;

  std::shared_ptr<spdlog::logger> logger_;
};
