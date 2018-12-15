/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace elf {

template <typename T>
class TypeNameT;

#define TYPE_NAME_CLASS(T)      \
  template <>                   \
  class TypeNameT<T> {          \
   public:                      \
    static std::string name() { \
      return #T;                \
    }                           \
  };

TYPE_NAME_CLASS(float);
TYPE_NAME_CLASS(double);
TYPE_NAME_CLASS(int64_t);
TYPE_NAME_CLASS(int32_t);
TYPE_NAME_CLASS(uint64_t);
TYPE_NAME_CLASS(uint32_t);

struct Size {
 public:
  Size(std::initializer_list<int> l) : sz_(l) {}
  Size(const std::vector<int>& l) : sz_(l) {}
  Size(const Size& s) : sz_(s.sz_) {}
  Size(int s) {
    sz_.push_back(s);
  }
  Size() {}
  friend bool operator==(const Size& s1, const Size& s2) {
    if (s1.size() != s2.size())
      return false;

    for (size_t i = 0; i < s1.size(); ++i) {
      if (s1[i] != s2[i])
        return false;
    }
    return true;
  }

  size_t nelement() const {
    int n = 1;
    for (const int& v : sz_)
      n *= v;
    return n;
  }

  const std::vector<int>& vec() const {
    return sz_;
  }
  int operator[](int i) const {
    return sz_[i];
  }
  size_t size() const {
    return sz_.size();
  }

  Size getContinuousStrides(int type_size) const {
    // size to stride.
    std::vector<int> prod(sz_.size(), 1);
    for (int i = sz_.size() - 1; i >= 1; --i) {
      prod[i - 1] = prod[i] * sz_[i];
    }
    for (auto& v : prod) {
      v *= type_size;
    }
    return Size(prod);
  }

  Size divide(int k) const {
    std::vector<int> res(sz_);
    for (int& r : res) {
      r /= k;
    }
    return Size(res);
  }

  size_t norder() const {
    return sz_.size();
  }
  std::string info() const {
    std::stringstream ss;
    ss << "(";
    for (const int& v : sz_)
      ss << v << ",";
    ss << ")";
    return ss.str();
  }

 private:
  std::vector<int> sz_;
};

} // namespace elf
