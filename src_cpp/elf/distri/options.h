/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sstream>
#include <string>
#include "elf/options/reflection_option.h"
#include "elf/distributed/options.h"
#include "elf/utils/reflection.h"
#include "elf/utils/utils.h"

namespace elf {

namespace cs {

DEF_STRUCT(Options)
  DEF_FIELD(int, server_num_state_pushed_per_thread, 1, "Number of states pushed per thread in the server side");
  DEF_FIELD_NODEFAULT(elf::msg::Options, net, "Network options");
  DEF_FIELD_NODEFAULT(elf::Options, base, "Base Options");
DEF_END

}  // namespace cs

}  // namespace elf
