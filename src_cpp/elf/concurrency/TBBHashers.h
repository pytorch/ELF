/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * Definition of various custom TBB hashers.
 *
 * TODO(maj): This is not particularly good practice, the maps requiring custom
 * hashers should themselves be parameterized.
 */

#pragma once

#include <stdint.h>

#include <thread>
#include <typeindex>

#include <tbb/concurrent_hash_map.h>

namespace tbb {
namespace interface5 {

template <>
inline size_t tbb_hasher<std::thread::id>(const std::thread::id& id) {
  return std::hash<std::thread::id>()(id);
}

template <>
inline size_t tbb_hasher<std::type_index>(const std::type_index& idx) {
  return std::hash<std::type_index>()(idx);
}

} // namespace interface5
} // namespace tbb
