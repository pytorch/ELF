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
#include "elf/base/options.h"
#include "elf/distributed/options.h"
#include "elf/utils/reflection.h"
#include "elf/utils/utils.h"

DEF_STRUCT(GameOptions)
  DEF_FIELD(int32_t, num_action, 10, "Number of actions");
  DEF_FIELD(int32_t, input_dim, 10, "Input dimention");
  
  DEF_FIELD_NODEFAULT(elf::msg::Options, net, "Network options");
  DEF_FIELD_NODEFAULT(elf::Options, base, "Base Options");
DEF_END

