/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "Pybind.h"

#include <stdint.h>

#include <iostream>

#include "tree_search_options.h"

#include "elf/options/reflection_option.h"
#include "elf/utils/pybind.h"
#include "elf/utils/reflection_utils.h"

namespace elf {
namespace ai {
namespace tree_search {

void registerPy(pybind11::module& m) {
  namespace py = pybind11;
  // auto ref = py::return_value_policy::reference_internal;

  PB_INIT(CtrlOptions)
    PB_FIELD(msec_start_time)
    PB_FIELD(msec_time_left)
    PB_FIELD(byoyomi)
    PB_FIELD(rollout_per_thread)
    PB_FIELD(msec_per_move)
  PB_END
}

} // namespace tree_search
} // namespace ai
} // namespace elf
