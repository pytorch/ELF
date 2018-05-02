/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "elf/concurrency/Counter.h"

class Notif {
 public:
  Notif() : _flag(false) {}

  const std::atomic_bool& flag() const {
    return _flag;
  }

  bool get() const {
    return _flag.load();
  }

  void notify() {
    _counter.increment();
  }

  void set() {
    _flag = true;
  }

  void wait(int n, std::function<void()> f = nullptr) {
    _flag = true;

    if (f == nullptr) {
      _counter.waitUntilCount(n);
    } else {
      while (true) {
        int current_cnt =
            _counter.waitUntilCount(n, std::chrono::microseconds(10));

        // LOG(INFO) << "current cnt = " << current_cnt
        //           << " n = " << n << std::endl;
        if (current_cnt >= n) {
          break;
        }
        f();
      }
    }
  }

  void reset() {
    _counter.reset();
    _flag = false;
  }

 private:
  std::atomic_bool _flag; // flag to indicate stop
  elf::concurrency::Counter<int> _counter;
};
