/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Counter.h"

namespace elf {
namespace concurrency {

// Explicit instantiations for various integer types.
template class Counter<int64_t>;
template class Counter<int32_t>;
template class Counter<bool>;

} // namespace concurrency
} // namespace elf
