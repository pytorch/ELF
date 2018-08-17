#pragma once

#include "elf/distributed/addrs.h"
#include "elf/distributed/shared_rw_buffer3.h"

#include "remote_common.h"
#include "game_context.h"
#include "game_interface.h"

namespace elf {
namespace remote {

class ReplyMsgs {
 public:
  void setSignature(const std::string& signature) {
    signature_ = signature;
  }

  void add(int idx, std::string&& s) {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    json j;
    j["idx"] = idx;
    j["signature"] = signature_;
    j["content"] = s;
    j_.push_back(j);
  }
  size_t size() {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    return j_.size();
  }

  std::string dump() {
    std::lock_guard<std::mutex> locker(reply_mutex_);
    /*
    if (j_.size() > 0) {
      std::cout << "dumping.. #records: " << j_.size() << std::endl;
    }
    */
    std::string ret = j_.dump();
    j_.clear();
    return ret;
  }

 private:
  std::mutex reply_mutex_;
  std::string signature_;
  json j_;
};

using RecvFunc = std::function<void (const std::string &)>;

class BatchReceiverInstance {
 public:
   BatchReceiverInstance(RecvFunc recv) : recv_(recv) {
     assert(recv_ != nullptr);
   }

   void start(const std::string &signature, const elf::shared::Options &netOptions) {
    client_.reset(new msg::Client(netOptions));
    reply_.setSignature(signature);

    auto receiver = [&](const std::string& recv_msg) -> int64_t {
      // Get data
      recv_(recv_msg);
      return -1;
    };

    auto sender = [&]() {
      // std::cout << timestr() << ", Dump data" << std::endl;
      return reply_.dump();
    };

    client_->setCallbacks(sender, receiver);
    client_->start();
   }

   ReplyMsgs &getReplyMsgs() { return reply_; }

 private:
   std::unique_ptr<msg::Client> client_;
   RecvFunc recv_ = nullptr;

   ReplyMsgs reply_;
};

class RemoteReceiver : public GCInterface {
 public:
  RemoteReceiver(const Options& options, const msg::Options& net)
      : GCInterface(options), netOptions_(net) {
    auto netOptions = msg::getNetOptions(options, net);
    netOptions.usec_sleep_when_no_msg = 1000000;
    netOptions.usec_resend_when_no_msg = -1;
    // netOptions.msec_sleep_when_no_msg = 2000;
    // netOptions.msec_resend_when_no_msg = 2000;
    netOptions.verbose = false;
    
    ctrl_client_.reset(new msg::Client(netOptions));
  }

  void start() override {
    auto receiver = [&](const std::string& recv_msg) -> int64_t {
      // Get data
      json j = json::parse(recv_msg);
      if (!j["valid"]) return -1;

      assert(j["port"].size() == kPortPerClient);

      auto netOptions = msg::getNetOptions(this->options(), netOptions_);
      netOptions.usec_sleep_when_no_msg = 1000;
      netOptions.usec_resend_when_no_msg = 10;
      // netOptions.msec_sleep_when_no_msg = 2000;
      // netOptions.msec_resend_when_no_msg = 2000;
      netOptions.verbose = false;

      for (size_t i = 0; i < kPortPerClient; ++i) {
        netOptions.port = j["port"][i];
        clients_[i]->start(j["signature"], netOptions);
      }
      return -1;
    };

    auto sender = [&]() {
      return "";
    };

    ctrl_client_->setCallbacks(sender, receiver);
    ctrl_client_->start();
  }

 protected:
  void initClients(RecvFunc recv_func) {
    for (size_t i = 0; i < kPortPerClient; ++i) {
      clients_.emplace_back(new BatchReceiverInstance(recv_func));
    }
  }

  size_t getNumClients() const { return clients_.size(); }

  void addReplyMsg(int client_idx, int message_idx, std::string &&msg) {
    clients_[client_idx]->getReplyMsgs().add(message_idx, std::move(msg));
  } 

 private:
  std::unique_ptr<msg::Client> ctrl_client_;
  std::vector<std::unique_ptr<BatchReceiverInstance>> clients_;

  const msg::Options netOptions_;
};

} // namespace remote
} // namespace elf
