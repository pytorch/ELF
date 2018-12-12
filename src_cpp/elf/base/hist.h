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

namespace elf {

// Accumulate history buffer.
template <typename T>
class HistT {
 public:
  HistT(size_t q_size, size_t vec_size, T undef_val = 0)
      : vec_size_(vec_size), undef_val_(undef_val) {
    q_ = std::vector<std::vector<T>>(q_size, std::vector<T>(vec_size_));
    assert(q_.size() > 0);
    assert(q_[0].size() == vec_size_);
  }

  void reset() {
    for (const auto &v : q_) {
      std::fill(v.begin(), v.end(), undef_val_);
    }
  }

  T* prepare() {
    // q_idx_ always points to the most recent entry.
    q_idx_ = (q_idx_ + 1) % q_.size();
    return &q_[q_idx_][0];
  }

  void extract(T* s) const {
    // one sample = dim per feature * time length
    size_t idx = q_idx_;
    for (size_t i = 0; i < q_.size(); ++i) {
      const auto &v = q_[idx];
      assert(v.size() == vec_size_);
      copy(v.begin(), v.end(), s);
      s += v.size();

      if (idx == 0) idx = q_.size() - 1;
      else idx --;
    }
  } 

  void extractHistBatch(T* s, int batchsize, int batch_idx) const {
    T* start = s + batch_idx * vec_size_;
    int stride = batchsize * vec_size_;
    size_t idx = q_idx_;
    for (size_t i = 0; i < q_.size(); ++i) {
      const auto &v = q_[idx];
      assert(v.size() == vec_size_);
      copy(v.begin(), v.end(), s);
      start += stride;

      if (idx == 0) idx = q_.size() - 1;
      else idx --;
    }
  }

 private:
  std::vector<std::vector<T>> q_;
  size_t q_idx_ = 0;
  size_t vec_size_;
  T undef_val_;
};

} // namespace elf
