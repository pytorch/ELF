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

#include <boost/range/adaptor/reversed.hpp>

namespace elf {

// Accumulate history buffer.
template <typename T>
class HistT {
 public:
  enum MemOrder { BATCH_HIST, HIST_BATCH };

  HistT(size_t q_size, size_t vec_size, MemOrder order)
      : vec_size_(vec_size), order_(order) {
    q_ = std::vector<std::vector<T>>(q_size, std::vector<T>(vec_size_));
    assert(q_.size() > 0);
    assert(q_[0].size() == vec_size_);
  }

  T* prepare() {
    q_idx_ = (q_idx_ + 1) % q_.size();
    return &q_[q_idx_][0];
  }

  void extract(T* s, int batchsize, int batch_idx) const {
    switch (order_) {
      case BATCH_HIST:
        ext_batch_hist(s, batch_idx);
        break;
      case HIST_BATCH:
        ext_hist_batch(s, batchsize, batch_idx);
        break;
      default:
        assert(false);
    }
  }

 private:
  std::vector<std::vector<T>> q_;
  size_t q_idx_ = 0;
  size_t vec_size_;
  MemOrder order_;

  void ext_batch_hist(T* s, int batch_idx) const {
    // one sample = dim per feature * time length
    T* start = s + batch_idx * vec_size_ * q_.size();
    for (const auto& v : boost::adaptors::reverse(q_)) {
      assert(v.size() == vec_size_);
      copy(v.begin(), v.end(), start);
      start += v.size();
    }
  }

  void ext_hist_batch(T* s, int batchsize, int batch_idx) const {
    T* start = s + batch_idx * vec_size_;
    int stride = batchsize * vec_size_;
    for (const auto& v : boost::adaptors::reverse(q_)) {
      assert(v.size() == vec_size_);
      copy(v.begin(), v.end(), s);
      start += stride;
    }
  }
};

} // namespace elf
