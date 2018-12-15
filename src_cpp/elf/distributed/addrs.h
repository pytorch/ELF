#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "../interface/options.h"
#include "options.h"
#include "shared_rw_buffer3.h"

namespace elf {
namespace msg {

inline elf::shared::Options getNetOptions(
    const elf::Options& options,
    const Options& netOpt) {
  static const std::unordered_map<std::string, std::string> gAddrs{
    // Add your own abbreviation and associated ipv4/v6 address
    // "name", "[192.168.0.1]" 
  };

  elf::shared::Options netOptions;
  if (netOpt.server_id != "") {
    auto it = gAddrs.find(netOpt.server_id);
    if (it != gAddrs.end()) {
      netOptions.addr = it->second;
    }
  }
  if (netOptions.addr == "") {
    netOptions.addr =
        (netOpt.server_addr == "" ? "localhost" : netOpt.server_addr);
  }

  netOptions.port = netOpt.port;
  netOptions.use_ipv6 = true;
  netOptions.verbose = options.verbose;
  netOptions.identity = options.job_id;

  return netOptions;
}

} // namespace msg
} // namespace elf
