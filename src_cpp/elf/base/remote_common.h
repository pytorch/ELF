#pragma once 

#include "elf/concurrency/ConcurrentQueue.h"

namespace elf {
namespace remote {

static constexpr int kPortPerClient = 2;

template <typename T>
using Queue = elf::concurrency::ConcurrentQueueMoodyCamel<T>;

inline std::string timestr() {
  return std::to_string(elf_utils::msec_since_epoch_from_now());
}

using RecvFunc = std::function<void (const std::string &)>;

} // namespace remote
} // namespace elf
