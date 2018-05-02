/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <type_traits>

#define MEMBER_CHECK(member)                                                  \
                                                                              \
  template <typename __T, typename = int>                                     \
  struct has_##member : std::false_type {};                                   \
                                                                              \
  template <typename __T>                                                     \
  struct has_##member<__T, decltype((void)__T::member, 0)> : std::true_type { \
  };

#define MEMBER_FUNC_CHECK(func)                            \
  template <typename __T>                                  \
  struct has_func_##func {                                 \
    template <typename __C>                                \
    static char test(decltype(&__C::func));                \
    template <typename __C>                                \
    static long test(...);                                 \
    enum { value = sizeof(test<__T>(0)) == sizeof(char) }; \
  };
