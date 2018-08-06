#pragma once

#include "elf/base/context.h"
#include "elf/base/dispatcher.h"
#include "record.h"

using Ctrl = elf::Ctrl;
using Addr = elf::Addr;
using MsgReply = int;
using ThreadedDispatcher = elf::ThreadedDispatcherT<MsgRequest, MsgReply>;

class DispatcherCallback {
 public:
  DispatcherCallback(ThreadedDispatcher* dispatcher) {
    using std::placeholders::_1;
    using std::placeholders::_2;

    dispatcher->Start(
        std::bind(&DispatcherCallback::OnReply, this, _1, _2),
        std::bind(&DispatcherCallback::OnFirstSend, this, _1, _2));
  }

  void OnFirstSend(const Addr&, MsgRequest*) { }

  std::vector<bool> OnReply(
      const std::vector<MsgRequest>& requests,
      std::vector<MsgReply>* p_replies) {
    (void)requests;
    assert(p_replies != nullptr);
    auto& replies = *p_replies;
    std::vector<bool> next_session(replies.size(), false);
    return next_session;
  }
};
