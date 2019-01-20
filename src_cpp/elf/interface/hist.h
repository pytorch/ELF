/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>
#include <vector>
#include <functional>

namespace elf {

template <typename T>
class HistTrait;

template <typename T>
class HistTrait<std::vector<T>> {
 public:
  HistTrait(int vec_size, T undef_value = 0) 
    : vec_size_(vec_size), undef_value_(undef_value) {
  }

  void Initialize(std::vector<T> &v) const {
    v.resize(vec_size_);
    std::fill(v.begin(), v.end(), undef_value_);
  } 

  size_t Extract(const std::vector<T> &v, T *s) const {
    copy(v.begin(), v.end(), s);
    return v.size();
  }

  T getUndefValue() const { return undef_value_; }

 private:
  int vec_size_;
  T undef_value_;
};

class HistIterator {
 public:
  explicit HistIterator(size_t q_idx, size_t q_size) 
    : q_idx_(q_idx), q_size_(q_size) {
    if (q_size_ > 0) q_idx_ %= q_size_;
  }

  HistIterator() {}

  HistIterator &operator++() {
    q_idx_ ++;
    if (q_idx_ >= q_size_) q_idx_ = 0;
    return *this;
  }

  friend HistIterator operator+(const HistIterator &h, size_t l) {
    HistIterator it = h;
    it.q_idx_ += l;
    it.q_idx_ %= it.q_size_;
    return it;
  }

  friend HistIterator operator-(const HistIterator &h, size_t l) {
    HistIterator it = h;
    it.q_idx_ += it.q_size_ - (l % it.q_size_);
    it.q_idx_ %= it.q_size_;
    return it;
  }

  friend size_t operator-(const HistIterator &it1, const HistIterator &it2) {
    assert(it1.q_size_ == it2.q_size_);
    size_t delta = it1.q_size_ + it1.q_idx_ - it2.q_idx_;
    return delta % it1.q_size_;
  }

  HistIterator &operator--() {
    if (q_idx_ == 0) q_idx_ = q_size_ - 1;
    else q_idx_ --;
    return *this;
  }

  friend bool operator==(const HistIterator &it1, const HistIterator &it2) {
    assert(it1.q_size_ == it2.q_size_);
    return it1.q_idx_ == it2.q_idx_;
  }

  friend bool operator!=(const HistIterator &it1, const HistIterator &it2) {
    return ! (it1 == it2);
  }

  size_t operator*() {
    return q_idx_;
  }

 protected:
  size_t q_idx_ = 0;
  size_t q_size_ = 0;
};

enum ExtractChoice { FULL_ONLY, CURR_SIZE };

// Accumulate history buffer.
template <typename T>
class HistT {
 public:
  using Initializer = std::function<void (T &)>;

  class HistInterval {
   public:
    HistInterval(const HistT<T> &h) 
      : h_(h) {
        b_ = h_.begin();
        e_ = h_.end();
    }

    HistInterval(const HistT<T> &h, HistIterator b, size_t l) 
      : h_(h) {
        b_ = b;
        e_ = b + l;
    }

    // From oldest to most recent.
    void forward(std::function<void (const T &)> extractor) const {
      for (auto it = b_; it != e_; ++it) {
        extractor(h_.q_[*it]);
      }
    }

    // From most recent to oldest
    void backward(std::function<void (const T &)> extractor) const {
      auto it = e_;
      do {
        --it;
        extractor(h_.q_[*it]);
      } while (it != b_);
    }

    size_t length() const { return e_ - b_; }

    HistInterval sample(int l, std::mt19937 &rng) const {
      size_t span = e_ - b_;
      size_t idx = rng() % (span + 1 - l);
      return HistInterval(h_, b_ + idx, l);
    }

   private:
    const HistT<T> &h_;
    HistIterator b_;
    HistIterator e_;
  };

  HistT(size_t q_size) {
    q_ = std::vector<T>(q_size + 1);
    curr_size_ = 0;
  }

  void reset(Initializer initializer) {
    if (initializer != nullptr) {
      for (auto &v : q_) {
        initializer(v);
      }
    }
    curr_size_ = 0;
  }

  size_t maxlen() const { return q_.size() - 1; }
  size_t currSize() const { return curr_size_; }

  bool isFull() const { return maxlen() == curr_size_; }

  T &push(T &&v) {
    // q_idx_ always points to the most recent entry.
    q_idx_ = (q_idx_ + 1) % q_.size();
    q_[q_idx_] = std::move(v);
    if (curr_size_ < maxlen()) curr_size_ ++;
    return q_[q_idx_];
  }

  HistIterator begin() const {
    size_t idx = q_idx_ + q_.size() - curr_size_ + 1;
    return HistIterator(idx, q_.size());
  }

  HistIterator end() const {
    return HistIterator(q_idx_ + 1, q_.size());
  }

  HistInterval getInterval() const {
    return HistInterval(*this);
  }

  HistInterval getEmptyInterval() const {
    return HistInterval(*this, end(), 0);
  }

 private:
  std::vector<T> q_;
  size_t q_idx_ = 0;
  size_t curr_size_ = 0;
};

} // namespace elf
