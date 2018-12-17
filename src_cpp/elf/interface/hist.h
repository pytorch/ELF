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

 private:
  int vec_size_;
  T undef_value_;
};

// Accumulate history buffer.
template <typename T>
class HistT {
 public:
  using Initializer = std::function<void (T &)>;

  HistT(size_t q_size) {
    q_ = std::vector<T>(q_size);
  }

  void reset(Initializer initializer) {
    if (initializer != nullptr) {
      for (auto &v : q_) {
        initializer(v);
      }
    }
  }

  size_t size() const { return q_.size(); }

  T &push(T &&v) {
    // q_idx_ always points to the most recent entry.
    q_idx_ = (q_idx_ + 1) % q_.size();
    q_[q_idx_] = std::move(v);
    return q_[q_idx_];
  }

  size_t accumulate(std::function<size_t (const T &)> func) const {
    size_t sz = 0;
    for (const T &v : q_) {
      sz += func(v);
    }
    return sz;
  }

  // From oldest to most recent.
  template <typename S>
  void extractForward(S* s, size_t (T::*extractor)(S *) const) const {
    auto f = [=](const T &v, S *s) {
      return (v.*extractor)(s);
    };
    this->template extractForward<S>(s, f);
  }

  template <typename S>
  void extractForward(S* s, std::function<size_t (const T &, S *)> extractor) const {
    assert(extractor != nullptr);
    // one sample = dim per feature * time length
    size_t idx = q_idx_;

    for (size_t i = 0; i < q_.size(); ++i) {
      idx ++;
      if (idx >= q_.size()) idx = 0;

      const T &v = q_[idx];
      s += extractor(v, s);
    }
  } 

  // From most recent to oldest.
  template <typename S>
  void extractReverse(S* s, size_t (T::*extractor)(S *) const) const {
    auto f = [=](const T &v, S *s) {
      return (v.*extractor)(s);
    };
    this->template extractReverse<S>(s, f);
  }

  template <typename S>
  void extractReverse(S* s, std::function<size_t (const T &, S *)> extractor) const {
    assert(extractor != nullptr);
    // one sample = dim per feature * time length
    size_t idx = q_idx_;
    for (size_t i = 0; i < q_.size(); ++i) {
      const T &v = q_[idx];
      s += extractor(v, s);

      if (idx == 0) idx = q_.size() - 1;
      else idx --;
    }
  } 

  /* 
  void extractHistBatch(S* s, int batchsize, int batch_idx, Extractor extractor) const {
    assert(extractor != nullptr);

    S* start = s + batch_idx * vec_size_;
    int stride = batchsize * vec_size_;
    size_t idx = q_idx_;
    for (size_t i = 0; i < q_.size(); ++i) {
      const T &v = q_[idx];
      extractor(v, s);
      start += stride;

      if (idx == 0) idx = q_.size() - 1;
      else idx --;
    }
  }
  */

 private:
  std::vector<T> q_;
  size_t q_idx_ = 0;
};

} // namespace elf
