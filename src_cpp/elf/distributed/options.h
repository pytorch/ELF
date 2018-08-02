#pragma once

#include "../utils/reflection.h"

namespace elf {
namespace msg {

DEF_STRUCT(Options)
DEF_FIELD(std::string, server_id, "", "Server id");
DEF_FIELD(std::string, server_addr, "", "Server address");
DEF_FIELD(int, port, 0, "Server port");
DEF_END

} // namespace msg
} // namespace elf
