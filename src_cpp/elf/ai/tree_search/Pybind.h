/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <pybind11/pybind11.h>

namespace elf {
namespace ai {
namespace tree_search {

void registerPy(pybind11::module& m);

} // namespace tree_search
} // namespace ai
} // namespace elf
