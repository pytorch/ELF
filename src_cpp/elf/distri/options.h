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
#include "elf/interface/options.h"
#include "elf/options/reflection_option.h"
#include "elf/distributed/options.h"
#include "elf/utils/reflection.h"
#include "elf/utils/utils.h"

namespace elf {

namespace cs {

DEF_STRUCT(TrainCtrlOptions) 
  DEF_FIELD(int, num_reader, 50, "number of reader threads");
  DEF_FIELD(int, q_min_size, 10, "min number of entries in each queue");
  DEF_FIELD(int, q_max_size, 1000, "max number of entries in each queue");
DEF_END

DEF_STRUCT(ClientManagerOptions)
  DEF_FIELD(int, max_num_threads, 100, "Max number of threads");
  DEF_FIELD(int, client_max_delay_sec, 1200, "max delay for each client. The client is regarded as dead if it doesn't respond for more than that amount of time"); 
  DEF_FIELD(int, expected_num_clients, 1200, "Expected number of total clients"); 
  DEF_FIELD_NODEFAULT(std::vector<float>, client_type_ratios, "client type ratios");
  DEF_FIELD_NODEFAULT(std::vector<int>, client_type_limits, "maximal number of clients for each type, -1 means no limit");

  std::string info() const {
    std::stringstream ss;
    ss << "[#max_th=" << max_num_threads << "]"
       << "[#client_delay=" << client_max_delay_sec << "]"
       << "[expected_#clients=" << expected_num_clients << "]";
    ss << "[expected_ratio=";
    for (auto f : client_type_ratios) ss << f << ",";
    ss << "]";

    ss << "[limits=";
    for (auto f : client_type_limits) ss << f << ",";
    ss << "]";

    return ss.str();
  }
DEF_END

DEF_STRUCT(Options)
  DEF_FIELD_NODEFAULT(TrainCtrlOptions, tc_opt, "TrainCtrl options");
  DEF_FIELD_NODEFAULT(ClientManagerOptions, cm_opt, "ClientManager options");
  DEF_FIELD_NODEFAULT(elf::msg::Options, net, "Network options");
  DEF_FIELD_NODEFAULT(elf::Options, base, "Base Options");
DEF_END

}  // namespace cs

}  // namespace elf
