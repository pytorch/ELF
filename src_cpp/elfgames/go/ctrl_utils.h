/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include <mutex>
#include "record.h"

struct RecordBuffer {
 public:
  RecordBuffer() {}
  RecordBuffer(const RecordBuffer&) = delete;
  RecordBuffer(RecordBuffer&&) = default;

  void resetPrefix(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!records_.empty()) {
      saveCurrent();
      clear();
    }
    num_saved_ = 0;
    prefix_ = prefix;
  }

  const std::string& prefix() const {
    return prefix_;
  }
  std::string prefix_save_counter() const {
    return prefix_ + "-" + std::to_string(num_saved_);
  }

  void feed(const Record& r) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (r.offline)
      offline_records_.push_back(r);
    else
      records_.push_back(r);
  }

  void saveCurrent(size_t num_record_per_segment = 1000) {
    auto it = records_.begin();
    auto it_end = records_.end();
    int counter = 0;

    do {
      size_t n = it_end - it;
      auto it2 =
          (n > num_record_per_segment) ? (it + num_record_per_segment) : it_end;

      std::string games = Record::dumpBatchJsonString(it, it2);

      std::ofstream oo(
          prefix_ + "-" + std::to_string(num_saved_) + "-" +
          std::to_string(counter) + ".json");
      oo << games;
      counter++;
      it = it2;
    } while (it != it_end);

    num_saved_++;
  }

  void clear() {
    records_.clear();
    offline_records_.clear();
  }

 private:
  std::mutex mutex_;
  std::vector<Record> records_;
  std::vector<Record> offline_records_;
  int num_saved_ = 0;
  std::string prefix_;
};

enum FeedResult {
  NOT_SELFPLAY,
  NOT_EVAL,
  VERSION_MISMATCH,
  NOT_REQUESTED,
  OLD_REQUESTED,
  FEEDED
};
