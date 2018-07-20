/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * The basic contract for all ELF concurrent queue implementations is:
 *
 * void push(const T& value)
 *   Pushes the value into the queue
 *
 * void pop(T* value)
 *   Blocking pop that stores the popped value in the given pointer
 *
 * bool pop(T* value, std::chrono::duration timeout)
 *   Same as above, but only waits for the given timeout duration.
 *   If the timeout duration is reached, then we return false and do not
 *   store anything in the given pointer.
 *
 * We define the following classes:
 *
 * ConcurrentQueueMoodyCamel<T> (aliased to ConcurrentQueue<T>)
 *   The default implementation, backed by moodycamel::blockingconcurrentqueue.
 *
 * ConcurrentQueueTBB<T>
 *   An alternative implementation, backed by tbb::concurrent_queue.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <blockingconcurrentqueue.h>
#include <tbb/concurrent_queue.h>

namespace elf {
namespace concurrency {

// moodycamel internally maintains a bunch of sub-queues for each producer
// thread and sometimes is not fair (the consumer.might always pick the data
// from a particular thread). Therefore, we amend it with a deque, which makes
// it only works for a single consumer.
template <typename T>
class ConcurrentQueueMoodyCamel {
 public:
  using value_type = T;

  void push(const T& value) {
    q_.enqueue(value);
  }

  void pop(T* value) {
    _check_consumer();
    if (_prefetch(value))
      return;
    q_.wait_dequeue(*value);
  }

  template <typename Rep, typename Period>
  bool pop(T* value, std::chrono::duration<Rep, Period> timeout) {
    _check_consumer();
    if (_prefetch(value))
      return true;
    return q_.wait_dequeue_timed(*value, timeout);
  }

 private:
  using QueueT = moodycamel::BlockingConcurrentQueue<T>;
  QueueT q_;
  std::deque<T> buffer_;

  std::thread::id single_consumer_;
  bool no_consumer_ = true;

  void _check_consumer() {
    if (no_consumer_) {
      single_consumer_ = std::this_thread::get_id();
      no_consumer_ = false;
    } else {
      assert(single_consumer_ == std::this_thread::get_id());
    }
  }

  bool _prefetch(T* v) {
    T value;
    while (q_.wait_dequeue_timed(value, std::chrono::microseconds(0))) {
      buffer_.push_back(value);
    }

    if (buffer_.empty())
      return false;

    *v = buffer_.front();
    buffer_.pop_front();
    return true;
  }
};

template <typename T>
class ConcurrentQueueTBB {
 public:
  using value_type = T;

  void push(const T& value) {
    q_.push(value);
  }

  void pop(T* value) {
    while (true) {
      if (q_.try_pop(*value)) {
        return;
      }
    }
  }

  template <typename Rep, typename Period>
  bool pop(T* value, std::chrono::duration<Rep, Period> timeout) {
    if (!q_.try_pop(*value)) {
      // Sleep would not efficiently return the element.
      std::this_thread::sleep_for(timeout);
      if (!q_.try_pop(*value)) {
        return false;
      }
    }
    return true;
  }

 private:
  using QueueT = tbb::concurrent_queue<T>;
  QueueT q_;
};

// Define the moodycamel queue to be the default implementation
template <typename T>
using ConcurrentQueue = ConcurrentQueueMoodyCamel<T>;

} // namespace concurrency
} // namespace elf
