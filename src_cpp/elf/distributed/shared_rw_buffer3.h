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
  // Receive message, and return RecvStatus, std::string * returns the content
  // it obtained.
  virtual RecvStatus onReceive(std::string*) = 0;

  // Send message given the content it receives. Derived class needs to deal
  // with case that the content is nullptr (error happens).
  virtual int64_t onSend(const std::string*) = 0;

  int64_t getSeq() const {
    return seq_;
  }

 private:
  void _send(const std::string* msg) {
    int64_t msg_seq = onSend(msg);
    if (msg_seq >= 0 && msg_seq != seq_) {
      std::cout << "Warning! The sequence number [" << msg_seq
                << "] in the msg is different from " << seq_ << std::endl;
    } else {
      seq_ = msg_seq + 1;
    }
    usec_since_last_sent_ = elf_utils::usec_since_epoch_from_now();
  }

  void onRecvNoContent(uint64_t dt) {
    if (options_.verbose) {
      std::cout << elf_utils::now() << ", " << name_
                << ", no message, seq=" << seq_ << ", since_last_usec=" << dt
                << std::endl;
    }

    if (options_.usec_resend_when_no_msg < 0 ||
        dt < (uint64_t)options_.usec_resend_when_no_msg) {
      if (options_.verbose) {
        std::cout << name_ << ", sleep for " << options_.usec_sleep_when_no_msg
                  << " usec .. " << std::endl;
      }
      std::this_thread::sleep_for(
          std::chrono::microseconds(options_.usec_sleep_when_no_msg));
    } else {
      if (options_.verbose) {
        std::cout << ", no reply for too long (" << dt << '>'
                  << options_.usec_resend_when_no_msg << " usec), resending"
                  << std::endl;
      }
      _send(nullptr);
    }
  }

  void onRecvOk(uint64_t dt, const std::string& smsg) {
    if (options_.verbose) {
      std::cout << elf_utils::now() << ", " << name_
                << ", In reply func: Message got. since_last_usec=" << dt
                << ", seq=" << seq_ << std::endl;
    }
    _send(&smsg);
  }

  void main_loop() {
    uint64_t now = elf_utils::usec_since_epoch_from_now();

    std::string smsg;
    RecvStatus recv_status = onReceive(&smsg);
    auto dt = now - usec_since_last_sent_;

    switch (recv_status) {
      case RECV_OK:
        onRecvOk(dt, smsg);
        break;
      case RECV_NO_MSG:
        onRecvNoContent(dt);
        break;
      case RECV_ERROR:
        throw std::runtime_error(name_ + " receive error!!");
    }
  }

 protected:
  elf::shared::Options options_;
  std::shared_ptr<spdlog::logger> logger_;

 private:
  std::unique_ptr<std::thread> thread_;
  std::atomic_bool done_;

  const std::string name_;

  uint64_t usec_since_last_sent_ = elf_utils::usec_since_epoch_from_now();
  int64_t seq_ = 0;
};

class Server : public Base {
 public:
  using ProcessFunc = std::function<
      bool(const std::string& identity, const std::string& recv_msg)>;

  using ReplyFunc =
      std::function<bool(const std::string& identity, std::string* reply_msg)>;

  Server(const elf::shared::Options& opt)
      : Base("elf::msg::Server", opt),
        receiver_(opt.port, opt.use_ipv6),
        rng_(time(NULL)) {}

  void setCallbacks(ProcessFunc proc_func, ReplyFunc replier = nullptr, ProcessFunc ctrl_func = nullptr) {
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
  ProcessFunc ctrl_func_ = nullptr;

  // Current identity;
  std::string curr_identity_;
  std::string curr_title_;

  int client_size_ = 0;
  int num_package_ = 0, num_failed_ = 0, num_skipped_ = 0;

 protected:
  RecvStatus onReceive(std::string* msg) override {
    if (!receiver_.recv_noblock(&curr_identity_, &curr_title_, msg)) {
      curr_identity_.clear();
      curr_title_.clear();
      return RECV_NO_MSG;
    } else {
      return RECV_OK;
    }
  }

  // Send message given the content it receives. Derived class needs to deal
  // with case that the content is nullptr (error happens).
  int64_t onSend(const std::string* msg) override {
    if (msg == nullptr)
      return -1;

    if (curr_title_ == "ctrl") {
      if (ctrl_func_ != nullptr) {
        ctrl_func_(curr_identity_, *msg);
      }
      client_size_++;
      if (options_.verbose) {
        std::cout << elf_utils::now() << " Ctrl from " << curr_identity_ << "["
                  << client_size_ << "]: " << *msg << std::endl;
      }
      // receiver_.send(identity, "ctrl", "");
    } else if (curr_title_ == "content") {
      if (!proc_func_(curr_identity_, *msg)) {
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

    // Send reply if there is any.
    if (replier_ != nullptr) {
      std::string reply;
      if (replier_(curr_identity_, &reply)) {
        receiver_.send(curr_identity_, "reply", reply);
      }
    }
    return -1;
  }
};

class Client : public Base {
 public:
  using RecvFunc = std::function<int64_t(const std::string&)>;
  using SendFunc = std::function<std::string()>;

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

  void setCallbacks(SendFunc send_func, RecvFunc recv_func) {
    send_func_ = send_func;
    recv_func_ = recv_func;
  }

 protected:
  std::unique_ptr<elf::shared::Writer> writer_;

  SendFunc send_func_ = nullptr;
  RecvFunc recv_func_ = nullptr;

  RecvStatus onReceive(std::string* msg) override {
    if (!writer_->getReplyNoblock(msg)) {
      return RECV_NO_MSG;
    } else {
      return RECV_OK;
    }
  }

  // Send message given the content it receives. Derived class needs to deal
  // with case that the content is nullptr (error happens).
  int64_t onSend(const std::string* msg) override {
    int64_t msg_seq = getSeq();
    if (msg != nullptr) {
      msg_seq = recv_func_(*msg);
    }
    assert(send_func_ != nullptr);
    std::string s = send_func_();
    writer_->Insert(s);
    return msg_seq;
  }
};

} // namespace msg

} // namespace elf
