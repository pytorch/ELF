#pragma once

#include <functional>
#include <string>
#include <sstream>

namespace comm {

enum ReplyStatus { DONE_ONE_JOB = 0, SUCCESS, FAILED, UNKNOWN };
using SuccessCallback = std::function<void ()>;

struct WaitOptions {
  int batchsize = 1;

  // If timeout_usec > 0, an incomplete batch of
  // size >= min_batchsize will be returned.
  int timeout_usec = 0;
  bool min_batchsize = 0;

  WaitOptions(int batchsize, int timeout_usec = 0, int min_batchsize = 0)
      : batchsize(batchsize),
        timeout_usec(timeout_usec),
        min_batchsize(min_batchsize) {}

  std::string info() const {
    std::stringstream ss;
    ss << "[bs=" << batchsize << "][timeout_usec=" << timeout_usec
       << "][min_bs=" << min_batchsize << "]";
    return ss.str();
  }

  friend bool operator==(const WaitOptions &op1, const WaitOptions &op2) {
    return op1.batchsize == op2.batchsize && op1.timeout_usec == op2.timeout_usec 
      && op1.min_batchsize == op2.min_batchsize;
  }
};

struct SendOptions {
  // Specify labels of this msg.
  // For each label, the message will be sent to a receiver with this label.
  // For example, if a message carrys the labels ["actor", "train"]
  // , and there are four receivers, each with label:
  //    1. "actor"
  //    2. "train"
  //    3. "actor"
  //    4. "train"
  // Then the message will be sent to (1, 2), (1, 4), (2, 3), (3, 4)
  // with equal probability.
  std::vector<std::string> labels;
};

struct RecvOptions {
  // A receiver will only honor messags that matches its label
  std::string label;
  WaitOptions wait_opt;

  RecvOptions(
      const std::string& label,
      int batchsize,
      int timeout_usec = 0,
      int min_batchsize = 0)
      : label(label), wait_opt(batchsize, timeout_usec, min_batchsize) {}

  friend bool operator==(const RecvOptions &op1, const RecvOptions &op2) {
    return op1.label == op2.label && op1.wait_opt == op2.wait_opt;
  }
};

}  // namespace comm
