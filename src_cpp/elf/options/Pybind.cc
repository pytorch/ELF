/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "Pybind.h"

#include "OptionMap.h"
#include "OptionSpec.h"

namespace elf {
namespace options {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;

  OptionMap::registerPy(m);
  OptionSpec::registerPy(m);
}

} // namespace options
} // namespace elf
