/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/pybind11.h>

#include "Pybind.h"

PYBIND11_MODULE(_elf, m) {
  elf::registerPy(m);
}
