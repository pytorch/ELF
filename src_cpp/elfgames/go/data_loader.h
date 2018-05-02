/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "elf/distributed/shared_rw_buffer2.h"
#include "record.h"

class DataOfflineLoaderJSON {
 public:
  using RQ = elf::shared::ReaderQueuesT<Record>;

  DataOfflineLoaderJSON(RQ& rq, const std::vector<std::string>& json_files)
      : rq_(rq), json_files_(json_files), rng_(time(NULL)) {}

  void start() {
    std::vector<std::thread> threads;
    int n = rq_.nqueue();
    std::atomic<int> count(0);

    for (const auto& f : json_files_) {
      std::cout << "DataOfflineLoaderJSON: Reading: " << f << std::endl;
      threads.emplace_back([f, n, this, &count]() {
        std::vector<Record> records;
        if (!Record::loadBatchFromJsonFile(f, &records)) {
          std::cout << "DataOfflineLoaderJSON: Error reading " << f
                    << std::endl;
          return;
        }
        for (auto&& r : records) {
          rq_[count.load() % n]->Insert(std::move(r));
          count++;
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    std::cout << "Save the records to ReaderQueues." << std::endl;
  }

 private:
  RQ& rq_;
  std::vector<std::string> json_files_;
  std::mt19937 rng_;
};

class DataOnlineLoader {
 public:
  using RQ = elf::shared::RQInterface;
  using ReplyFunc = elf::shared::Reader::ReplyFunc;
  using StartFunc = elf::shared::Reader::StartFunc;

  DataOnlineLoader(RQ& rq, const elf::shared::Options& net_options) : rq_(rq) {
    auto curr_timestamp = time(NULL);
    const std::string database_name =
        "data-" + std::to_string(curr_timestamp) + ".db";
    _reader.reset(new elf::shared::Reader(database_name, net_options));
    std::cout << _reader->info() << std::endl;
  }

  void start(StartFunc start_func = nullptr, ReplyFunc replier = nullptr) {
    _reader->startReceiving(&rq_, start_func, replier);
  }

  ~DataOnlineLoader() {}

 private:
  RQ& rq_;
  std::unique_ptr<elf::shared::Reader> _reader;
};
