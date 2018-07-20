#pragma once

#include <time.h>

#include <iostream>
#include <memory>
#include <vector>

#include "../common/record.h"
#include "data_loader.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"

inline elf::shared::Options getNetOptions(
    const ContextOptions& contextOptions,
    const GameOptions& options) {
  elf::shared::Options netOptions;
  netOptions.addr =
      options.server_addr == "" ? "localhost" : options.server_addr;
  netOptions.port = options.port;
  netOptions.use_ipv6 = true;
  netOptions.verbose = options.verbose;
  netOptions.identity = contextOptions.job_id;

  return netOptions;
}
