#pragma once

#include "../utils/reflection.h"
#include "../utils/utils.h"

namespace elf {

DEF_STRUCT(Options)
DEF_FIELD(int, num_game_thread, 1, "#Num of game threads");
DEF_FIELD(int, batchsize, 1, "Batchsize");
DEF_FIELD(bool, verbose, false, "Verbose");
DEF_FIELD(int64_t, seed, 0L, "Seed");
DEF_FIELD(std::string, job_id, "", "Job Id");
DEF_FIELD(std::string, time_signature, "", "Time Signature");

Options() {
  time_signature = elf_utils::time_signature();
}
DEF_END

} // namespace elf
