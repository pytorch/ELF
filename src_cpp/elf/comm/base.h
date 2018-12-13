#pragma once

#include <functional>

namespace comm {

enum ReplyStatus { DONE_ONE_JOB = 0, SUCCESS, FAILED, UNKNOWN };
using SuccessCallback = std::function<void ()>;

}  // namespace comm
