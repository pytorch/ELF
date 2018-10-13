#pragma once

#include "elf/distributed/addrs.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "remote_common.h"
#include "game_context.h"
#include "game_interface.h"

#include <algorithm>
#include <iterator>

namespace elf {
namespace remote {

class _Client {
 public:
   using Ls = std::vector<std::string>;
   
   _Client(const elf::shared::Options &netOptions) {
     client_.reset(new msg::Client(netOptions));
   }

   void start(const Ls& labels, SendQ &send_q, RecvQ &recv_q) { 
     auto id = identity();
     send_q_ = &send_q.addQ(id, labels);
     recv_q_ = &recv_q.addQ(id, labels);
          
     auto receiver = [&](const std::string& recv_msg) -> int64_t {
       // Get data
       recv_q_->parseAdd(recv_msg);
       return -1;
     };

     auto sender = [&]() {
       // std::cout << timestr() << ", Dump data" << std::endl;
       return send_q_->dumpClear();
     };

     client_->setCallbacks(sender, receiver);
     client_->start();
   }

   std::string identity() const { return client_->identity(); }

 private:
   std::unique_ptr<msg::Client> client_;
   SendSingleInterface *send_q_ = nullptr;
   RecvSingleInterface *recv_q_ = nullptr;
};

// A lot of msg::Client. 
// 1.  Try connecting to the server side and build connections.
class Clients : public Interface {
 public:
  Clients(const shared::Options &netOptions, const std::vector<std::string> &labels) 
      : netOptions_(netOptions), labels_(labels) {
    std::sort(labels_.begin(), labels_.end());

    netOptions_.usec_sleep_when_no_msg = 1000000;
    netOptions_.usec_resend_when_no_msg = -1;
    // netOptions_.msec_sleep_when_no_msg = 2000;
    // netOptions_.msec_resend_when_no_msg = 2000;
    netOptions_.verbose = false;

    {
      json j;
      j["labels"] = labels_;
      netOptions_.hello_message = j.dump();
    }
    ctrl_client_.reset(new msg::Client(netOptions_));
    netOptions_.hello_message = "";

    auto receiver = [&](const std::string& recv_msg) -> int64_t {
      // Get data
      json j = json::parse(recv_msg);
      if (!j["valid"]) return -1;

      assert(j["port"].size() == kPortPerClient);

      auto netOptions = netOptions_;
      netOptions.usec_sleep_when_no_msg = 1000;
      netOptions.usec_resend_when_no_msg = 10;
      // netOptions.msec_sleep_when_no_msg = 2000;
      // netOptions.msec_resend_when_no_msg = 2000;

      for (size_t i = 0; i < kPortPerClient; ++i) {
        netOptions.port = j["port"][i];
        netOptions.identity = j["client_identity"][i];
        netOptions.no_prefix_on_identity = true;
        clients_.emplace_back(new _Client(netOptions));
        clients_.back()->start(j["labels"], send_q_, recv_q_);
      }
      return -1;
    };

    auto sender = [&]() {
      return "";
    };

    ctrl_client_->setCallbacks(sender, receiver);
    ctrl_client_->start();
  }

 private:
  shared::Options netOptions_;
  std::vector<std::string> labels_;

  std::unique_ptr<msg::Client> ctrl_client_;
  std::vector<std::unique_ptr<_Client>> clients_;
};

} // remote
} // elf
