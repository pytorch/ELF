/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * The Counter<IntT> class is a thread-safe integer counter.
 */

#pragma once

#include <stdint.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace elf {
namespace concurrency {

template <typename T>
class Counter {
 public:
  using value_type = T;

  Counter(T initialValue = 0) : count_(initialValue) {}

  /**
   * This method updates count using the predicate and returns the new count
   * (i.e. return count = predicate(count)).
   */
  template <typename PredicateT>
  T replace(PredicateT predicate) {
    std::unique_lock<std::mutex> lock(mutex_);
    count_ = predicate(count_);
    cv_.notify_all();
    return count_;
  }

  /**
   * This method blocks until predicate(count) is true, then returns the count.
   */
  template <typename PredicateT>
  T wait(PredicateT predicate) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this, &predicate]() { return predicate(this->count_); });
    return count_;
  }

  /**
   * This method blocks until predicate(count) is true or the timeout duration
   * has elapsed (whichever comes first), then returns the count.
   */
  template <typename PredicateT, typename Rep, typename Period>
  T wait(PredicateT predicate, std::chrono::duration<Rep, Period> timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, timeout, [this, &predicate]() {
      return predicate(this->count_);
    });
    return count_;
  }

  // Convenience methods follow.

  /**
   * This method increments count.
   */
  T increment(T increment = 1) {
    return replace([=](T value) { return value + increment; });
  }

  /**
   * This method sets count = newValue.
   */
  T set(T newValue) {
    return replace([=](T) { return newValue; });
  }

  /**
   * This method sets count = 0.
   */
  T reset() {
    return set(0);
  }

  /**
   * This method blocks until count >= expectedCount, then returns the count.
   */
  T waitUntilCount(T expectedCount) {
    return wait([=](T count) { return count >= expectedCount; });
  }

  /**
   * This method blocks until count >= expectedCount or the timeout duration
   * has elapsed (whichever comes first), then returns the count.
   */
  template <typename Rep, typename Period>
  T waitUntilCount(
      T expectedCount,
      std::chrono::duration<Rep, Period> timeout) {
    return wait([=](T count) { return count >= expectedCount; }, timeout);
  }

 private:
  T count_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

// Exempt the explicit instantiations in Counter.cc from compilation.
extern template class Counter<int64_t>;
extern template class Counter<int32_t>;
extern template class Counter<bool>;

class Switch : public Counter<bool> {
 public:
  Switch(bool initialValue = false) : Counter(initialValue) {}

  bool waitUntilValue(bool value) {
    return wait([=](bool currentValue) { return currentValue == value; });
  }

  template <typename Rep, typename Period>
  bool waitUntilValue(bool value, std::chrono::duration<Rep, Period> timeout) {
    return wait(
        [=](bool currentValue) { return currentValue == value; }, timeout);
  }

  bool waitUntilTrue() {
    return waitUntilValue(true);
  }

  template <typename Rep, typename Period>
  bool waitUntilTrue(std::chrono::duration<Rep, Period> timeout) {
    return waitUntilValue(true, timeout);
  }

  bool waitUntilFalse() {
    return waitUntilValue(false);
  }

  template <typename Rep, typename Period>
  bool waitUntilFalse(std::chrono::duration<Rep, Period> timeout) {
    return waitUntilValue(false, timeout);
  }
};

} // namespace concurrency
} // namespace elf
