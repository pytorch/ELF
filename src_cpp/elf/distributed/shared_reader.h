/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <time.h>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <shared_mutex>
#include <thread>

namespace elf {

namespace shared {

struct ReaderCtrl {
  size_t queue_min_size = 10;
  size_t queue_max_size = 1000;
  std::string info() const {
    std::stringstream ss;
    ss << "Queue [min=" << queue_min_size << "][max=" << queue_max_size << "]";
    return ss.str();
  }
};

template <typename T>
class ReaderQueueT {
 public:
  using ReaderQ = ReaderQueueT<T>;

  class Sampler {
   public:
    explicit Sampler(ReaderQ* r, std::mt19937* rng)
        : r_(r), lock_(r->rwMutex_), rng_(rng) {
      released_ = false;
    }
    Sampler(const Sampler&) = delete;
    Sampler(Sampler&& sampler)
        : r_(sampler.r_), released_(sampler.released_), rng_(sampler.rng_) {}

    const T* sample(int timeout_millisec = 100) {
      const auto& buf = r_->buffer_;

      if (buf.size() < r_->ctrl_.queue_min_size) {
        lock_.unlock();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(timeout_millisec));
        lock_.lock();
        if (buf.size() < r_->ctrl_.queue_min_size)
          return nullptr;
      }

      int idx = (*rng_)() % buf.size();
      return &buf[idx];
    }

    ~Sampler() {
      release();
    }

   private:
    void release() {
      if (!released_) {
        lock_.unlock();
        released_ = true;
      }
    }

    ReaderQ* r_;
    std::shared_lock<std::shared_mutex> lock_;
    bool released_ = true;
    std::mt19937* rng_ = nullptr;
  };

  ReaderQueueT(const ReaderCtrl& ctrl) : ctrl_(ctrl) {}

  Sampler getSampler(std::mt19937* rng) {
    return Sampler(this, rng);
  }

  // Return delta buffer size.
  int Insert(T&& v) {
    int delta = 0;
    {
      std::unique_lock<std::shared_mutex> lock(rwMutex_);
      buffer_.push_back(v);
      delta++;
      while (buffer_.size() > ctrl_.queue_max_size) {
        buffer_.pop_front();
        delta--;
      }
    }
    return delta;
  }

  void clear() {
    std::unique_lock<std::shared_mutex> lock(rwMutex_);
    buffer_.clear();
  }

  std::vector<T> Dump() const {
    std::vector<T> vec;
    {
      std::shared_lock<std::shared_mutex> lock(rwMutex_);
      vec.insert(vec.end(), buffer_.begin(), buffer_.end());
    }
    return vec;
  }

  size_t size() const {
    return buffer_.size();
  }

  std::string info() const {
    std::stringstream ss;
    ss << "ReaderQueue: " << ctrl_.info();
    return ss.str();
  }

 private:
  // TODO: update to std::shared_mutex with C++17
  mutable std::shared_mutex rwMutex_;

  std::deque<T> buffer_;

  ReaderCtrl ctrl_;
};

class RQInterface {
 public:
  struct InsertInfo {
    bool success = false;
    int delta = 0;
    int msg_size = 0;
  };

  // bool: whether the insert is successful
  // int: changes of the queue length.
  virtual InsertInfo Insert(const std::string&, int queue_idx) = 0;
  virtual InsertInfo Insert(const std::string&, std::mt19937* rng) = 0;
  virtual size_t nqueue() const = 0;
};

struct RQCtrl {
  int num_reader;
  ReaderCtrl ctrl;
};

// Many reader queues to prevent locking.
template <typename T>
class ReaderQueuesT : public RQInterface {
 public:
  using ReaderQueue = ReaderQueueT<T>;

  using ConvertFuncSingle = std::function<bool(const std::string&, T*)>;
  using ConvertFuncVector =
      std::function<bool(const std::string&, std::vector<T>*)>;

  ReaderQueuesT(const RQCtrl& reader_ctrl) : min_size_satisfied_(false) {
    // Make sure this is an even number.
    assert(reader_ctrl.num_reader % 2 == 0);
    min_size_per_queue_ = reader_ctrl.ctrl.queue_min_size;

    for (int i = 0; i < reader_ctrl.num_reader; ++i) {
      qs_.emplace_back(new ReaderQueue(reader_ctrl.ctrl));
    }
  }

  void setConverter(ConvertFuncSingle f) {
    converter_ = [f, this](const std::string& s, std::function<int()> g) {
      RQInterface::InsertInfo info;

      T v;
      if (!f(s, &v))
        return info;

      info = Insert(std::move(v), g);
      info.msg_size = s.size();

      return info;
    };
  }

  void setConverter(ConvertFuncVector f) {
    converter_ = [f, this](const std::string& s, std::function<int()> g) {
      RQInterface::InsertInfo info;

      std::vector<T> vs;
      if (!f(s, &vs))
        return info;

      info = Insert(std::move(vs), g);
      info.msg_size = s.size();

      return info;
    };
  }

  InsertInfo Insert(const std::string& msg, int idx) override {
    return converter_(msg, [=]() { return idx; });
  }

  InsertInfo Insert(const std::string& msg, std::mt19937* rng) override {
    return converter_(
        msg, [rng, this]() -> int { return (*rng)() % qs_.size(); });
  }

  InsertInfo Insert(std::vector<T>&& vs, std::function<int()> g) {
    RQInterface::InsertInfo info;

    int delta = 0;
    for (auto&& v : vs) {
      delta += qs_[g()]->Insert(std::move(v));
      inc_insertion_count();
    }

    info.success = true;
    info.delta = delta;
    info.msg_size = 0;

    return info;
  }

  InsertInfo Insert(T&& v, std::function<int()> g) {
    RQInterface::InsertInfo info;

    int delta = qs_[g()]->Insert(std::move(v));

    info.success = true;
    info.delta = delta;
    info.msg_size = 0;
    inc_insertion_count();

    return info;
  }

  InsertInfo Insert(T&& v, std::mt19937* rng) {
    return Insert(
        std::move(v), [rng, this]() -> int { return (*rng)() % qs_.size(); });
  }

  InsertInfo InsertWithParity(T&& v, std::mt19937* rng, bool parity) {
    return Insert(std::move(v), [rng, this, parity]() -> int {
      // When parity == true, only insert to odd entry.
      // When parity == false, only insert to even entry.
      int ii = (*rng)() % (qs_.size() / 2);
      return 2 * ii + (parity ? 1 : 0);
    });
  }

  void clear() {
    min_size_satisfied_ = false;
    for (auto& q : qs_) {
      q->clear();
    }
  }

  std::vector<T> dumpAll() const {
    std::vector<T> res;
    for (auto& q : qs_) {
      std::vector<T> this_res = q->Dump();
      res.insert(res.end(), this_res.begin(), this_res.end());
    }
    return res;
  }

  size_t nqueue() const override {
    return qs_.size();
  }

  ReaderQueue* operator[](size_t idx) {
    return qs_[idx].get();
  }

  typename ReaderQueue::Sampler getSampler(int idx, std::mt19937* rng) {
    if (!min_size_satisfied_.load()) {
      while (!sufficient_per_queue_size()) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
      }
      min_size_satisfied_ = true;
    }
    return qs_[idx]->getSampler(rng);
  }

  std::string info() const {
    if (qs_.empty())
      return std::string();
    std::stringstream ss;
    ss << "#Queue: " << qs_.size() << ", spec: " << qs_[0]->info()
       << ", Length: ";
    size_t total = 0;
    for (const auto& p : qs_) {
      ss << p->size() << ", ";
      total += p->size();
    }
    ss << "Total: " << total
       << ", MinSizeSatisfied: " << min_size_satisfied_.load();
    return ss.str();
  }

 private:
  std::vector<std::unique_ptr<ReaderQueue>> qs_;
  std::function<
      RQInterface::InsertInfo(const std::string&, std::function<int()>)>
      converter_;
  size_t min_size_per_queue_ = 0;
  std::atomic_bool min_size_satisfied_;
  size_t total_insertion_ = 0;

  void inc_insertion_count() {
    total_insertion_++;
    if (total_insertion_ % 1000 == 0) {
      std::cout << elf_utils::now()
                << ", ReaderQueue Insertion: " << total_insertion_ << std::endl;
    }
  }

  bool sufficient_per_queue_size() const {
    for (const auto& q : qs_) {
      if (q->size() < min_size_per_queue_)
        return false;
    }
    return true;
  }
};

} // namespace shared

} // namespace elf
