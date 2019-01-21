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

enum ExtractChoice { FULL_ONLY, CURR_SIZE };

template <typename H, typename T>
class IntervalT {
 public:
  IntervalT(H &h) 
    : h_(h) {
      b_ = 0;
      e_ = h_.currSize();
  }

  IntervalT(H &h, size_t b, size_t l) 
    : h_(h) {
      b_ = b;
      e_ = b + l;
  }

  // From oldest to most recent.
  void forward(std::function<void (T &)> extractor) const {
    for (auto i = b_; i != e_; ++i) {
      extractor(h_[i]);
    }
  }

  // From most recent to oldest
  void backward(std::function<void (T &)> extractor) const {
    auto i = e_;
    do {
      --i;
      extractor(h_[i]);
    } while (i != b_);
  }

  size_t length() const { return e_ - b_; }

  IntervalT<H, T> sample(size_t l, std::mt19937 &rng) const {
    size_t span = e_ - b_;
    size_t idx = rng() % (span + 1 - l);
    return IntervalT<H, T>(h_, b_ + idx, l);
  }

  IntervalT<const H, const T> toConst() const {
    return IntervalT<const H, const T>(h_, b_, e_ - b_);
  }

 private:
  H &h_;
  size_t b_, e_;
};


// Accumulate history buffer.
template <typename T>
class HistT {
 public:
  using Initializer = std::function<void (T &)>;
  using Interval = IntervalT<HistT<T>, T>;
  using IntervalC = IntervalT<const HistT<T>, const T>;

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

  // From the oldest to the newest.
  const T& operator[](size_t i) const { return q_[_offset(curr_size_ - 1 - i)]; }
  T& operator[](size_t i) { return q_[_offset(curr_size_ - 1 - i)]; }

  // From the newest to the oldest (newest(0) = last entry).
  const T& newest(size_t i) const { return q_[_offset(i)]; }
  T& newest(size_t i) { return q_[_offset(i)]; }

  IntervalC getIntervalC() const {
    return IntervalC(*this);
  }

  Interval getInterval() {
    return Interval(*this);
  }

  IntervalC getEmptyIntervalC() const {
    return IntervalC(*this, curr_size_, 0);
  }

 private:
  std::vector<T> q_;
  size_t q_idx_ = 0;
  size_t curr_size_ = 0;

  // Newest to oldest idx.
  size_t _offset(size_t i) const {
    assert(i < curr_size_);
    size_t idx = q_idx_ + q_.size() - i;
    if (idx >= q_.size()) idx -= q_.size();
    return idx;
  }
};

} // namespace elf
