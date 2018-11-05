/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <pybind11/pybind11.h>

namespace elfgames {
namespace tasks {

void registerPy(pybind11::module& m);

} // namespace tasks 
} // namespace elfgames
