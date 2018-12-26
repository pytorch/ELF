#pragma once
#include "../base/ctrl.h"
#include "../logging/IndexedLoggerFactory.h"
#include "shared_rw_buffer2.h"

namespace elf {

namespace msg {

using StartFunc = std::function<void()>;

class Base {
 public:
  enum RecvStatus { RECV_OK, RECV_NO_MSG, RECV_ERROR };

  Base(const std::string& name, const shared::Options& options)
      : options_(options),
        logger_(elf::logging::getLogger(name, "")),
        done_(false),
        name_(name) {}

  void start(StartFunc start_func = nullptr) {
    thread_.reset(new std::thread(
        [=](Base* p) {
          if (start_func != nullptr)
            start_func();
          while (!done_.load()) {
            p->main_loop();
          }
        },
        this));
  }

  virtual ~Base() {
    if (thread_.get() != nullptr) {
      std::cout << "Destroying elf::msg::Base ... " << std::endl;
      done_ = true;
      thread_->join();
      std::cout << "elf::msg::Base destroyed... " << std::endl;
    }
  }

 protected:
  // Receive message, and return RecvStatus.
  virtual RecvStatus onReceive() = 0;

  // Send message.
  // if onSend actually sends data return true, else return false;
  virtual bool onSend() = 0;

 private:
  void main_loop() {
    uint64_t now = elf_utils::usec_since_epoch_from_now();

    RecvStatus recv_status = onReceive();
    if (recv_status == RECV_ERROR) {
      throw std::runtime_error(name_ + " receive error!!");
    }
    auto dt = now - usec_last_sent_;
    bool received = (recv_status == RECV_OK);

    if (options_.verbose) {
      if (! received) {
        std::cout << elf_utils::now() << ", " << name_
          << ", no message, since_last_usec=" << dt
          << std::endl;
      } else {
        std::cout << elf_utils::now() << ", " << name_
          << ", In reply func: Message got. since_last_usec=" << dt
          << std::endl;
      }
    }

    bool sent = false;
    if (onSend()) {
      usec_last_sent_ = elf_utils::usec_since_epoch_from_now();
      sent = true;
    }

    if (! sent && ! received) {
      if (options_.verbose) {
        std::cout << name_ << ", sleep for " << options_.usec_sleep_when_no_msg
                  << " usec .. " << std::endl;
      }
      std::this_thread::sleep_for(
          std::chrono::microseconds(options_.usec_sleep_when_no_msg));
    }
  }

 protected:
  elf::shared::Options options_;
  std::shared_ptr<spdlog::logger> logger_;

  uint64_t usec_since_last_sent() const {
    return elf_utils::usec_since_epoch_from_now() - usec_last_sent_;
  }

 private:
  std::unique_ptr<std::thread> thread_;
  std::atomic_bool done_;

  const std::string name_;

  uint64_t usec_last_sent_ = elf_utils::usec_since_epoch_from_now();
};

enum ReplyStatus { NO_REPLY, MORE_REPLY, FINAL_REPLY };

class Server : public Base {
 public:
  // Return true if the message is processed correctly.
  // false if there is error during processing.
  using ProcessFunc = std::function<
      bool(const std::string& identity, const std::string& recv_msg)>;

  // Deal with first time control message.
  using CtrlFunc = std::function<
      void(const std::string& identity, const std::string& recv_msg)>;

  // Reply function. Return MORE_REPLY / FINAL_REPLY if there is a message to be sent.
  // Reply function will be called repeatedly until the return value is NO_REPLY or FINAL_REPLY.
  // identity was initially set to the current identity (can be empty string if there is no message received in this pass).
  //   and can be specified if we want to send the message to other identities.
  using ReplyFunc =
      std::function<ReplyStatus (std::string* identity, std::string* reply_msg)>;

  Server(const elf::shared::Options& opt)
      : Base("elf::msg::Server", opt),
        receiver_(opt.port, opt.use_ipv6),
        rng_(time(NULL)) {}

  void setCallbacks(ProcessFunc proc_func, ReplyFunc replier = nullptr, CtrlFunc ctrl_func = nullptr) {
    proc_func_ = proc_func;
    ctrl_func_ = ctrl_func;
    replier_ = replier;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "ZMQVer: " << elf::distri::s_version() << " Reader "
       << options_.info();
    return ss.str();
  }

 private:
  elf::distri::ZMQReceiver receiver_;
  std::mt19937 rng_;

  ProcessFunc proc_func_ = nullptr;
  ReplyFunc replier_ = nullptr;

  // Get called when the server first get data from the client.
  CtrlFunc ctrl_func_ = nullptr;

  // Current identity;
  std::string curr_identity_;
  std::string curr_title_;

  int client_size_ = 0;
  int num_package_ = 0, num_failed_ = 0, num_skipped_ = 0;

 protected:
  RecvStatus onReceive() override {
    std::string msg;
    if (!receiver_.recv_noblock(&curr_identity_, &curr_title_, &msg)) {
      curr_identity_.clear();
      curr_title_.clear();
      return RECV_NO_MSG;
    }

    if (curr_title_ == "ctrl") {
      if (ctrl_func_ != nullptr) {
        ctrl_func_(curr_identity_, msg);
      }
      client_size_++;
      if (options_.verbose) {
        std::cout << elf_utils::now() << " Ctrl from " << curr_identity_ << "["
          << client_size_ << "]: " << msg << std::endl;
      }
      // receiver_.send(identity, "ctrl", "");
    } else if (curr_title_ == "content") {
      if (!proc_func_(curr_identity_, msg)) {
        std::cout << "Msg processing error! from " << curr_identity_
          << std::endl;
        num_failed_++;
      } else {
        num_package_++;
      }
    } else {
      std::cout << elf_utils::now() << " Skipping unknown title: \""
        << curr_title_ << "\", identity: \"" << curr_identity_ << "\""
        << std::endl;
      num_skipped_++;
    }
    return RECV_OK;
  }

  // Send message given the content it receives. Derived class needs to deal
  // with case that the content is nullptr (error happens).
  bool onSend() override {
    // For server, if received == false then curr_identity_ is also empty.
    // Send reply if there is any.
    if (replier_ == nullptr) return false;

    bool sent = false;
    ReplyStatus status;
    do {
      std::string reply;
      std::string identity = curr_identity_;
      status = replier_(&identity, &reply);
      if (status != NO_REPLY) {
        // std::cout << "Send reply. size: " << reply.size() << ", id: " << identity << std::endl;
        receiver_.send(identity, "reply", reply);
        sent = true;
      }
    } while (status == MORE_REPLY);
    return sent;
  }
};

class Client : public Base {
 public:
  // The function is called if we receive message from the server.
  using RecvFunc = std::function<void (const std::string&)>;

  // Return MORE_REPLY or FINAL_REPLY if the function has message to be sent
  // It will be kept calleing until the return status becomes NO_REPLY or FINAL_REPLY.
  using SendFunc = std::function<ReplyStatus (std::string *)>;

  // IF for a long time no message is sent to the server, call this function
  // to send one additional (to keep alive).
  using TimerFunc = std::function<std::string()>;

  Client(const elf::shared::Options& netOptions)
      : Base("elf::msg::Client", netOptions) {
    writer_.reset(new elf::shared::Writer(netOptions));
    auto currTimestamp = time(NULL);
    logger_->info(
        "Writer info: {}, send ctrl with timestamp {} ",
        writer_->info(),
        currTimestamp);

    auto msg =
      netOptions.hello_message.size() > 0 ?
        netOptions.hello_message :
        std::to_string(currTimestamp);
    writer_->Ctrl(msg);
  }

  std::string identity() const {
    return writer_->identity();
  }

  void setCallbacks(SendFunc send_func, RecvFunc recv_func, TimerFunc timer_func = nullptr) {
    send_func_ = send_func;
    recv_func_ = recv_func;
    timer_func_ = timer_func;
  }

 protected:
  std::unique_ptr<elf::shared::Writer> writer_;

  SendFunc send_func_ = nullptr;
  RecvFunc recv_func_ = nullptr;
  TimerFunc timer_func_ = nullptr;

  RecvStatus onReceive() override {
    std::string msg;
    if (!writer_->getReplyNoblock(&msg)) {
      return RECV_NO_MSG;
    } else {
      recv_func_(msg);
      return RECV_OK;
    }
  }

  // Send message given the content it receives. Derived class needs to deal
  // with case that the content is nullptr (error happens).
  bool onSend() override {
    assert(send_func_ != nullptr);

    bool sent = false;
    ReplyStatus status;

    do {
      std::string s;
      status = send_func_(&s);
      if (status != NO_REPLY) {
        writer_->Insert(s);
        sent = true;
      }
    } while (status == MORE_REPLY);

    if (! sent && timer_func_ != nullptr && usec_since_last_sent() >= 1000000) {
      // If time permits, we send a timer message.
      // If nothing happens in 1s we send a timer.
      writer_->Insert(timer_func_());
      sent = true;
    }

    return sent;
  }
};

} // namespace msg

} // namespace elf
