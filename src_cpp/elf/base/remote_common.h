#pragma once 

#include "elf/concurrency/ConcurrentQueue.h"

namespace elf {
namespace remote {

static constexpr int kPortPerClient = 2;
static constexpr int kPortPerServer = 4;

template <typename T>
using Queue = elf::concurrency::ConcurrentQueueMoodyCamelNoCheck<T>;

inline std::string timestr() {
  return std::to_string(elf_utils::msec_since_epoch_from_now());
}

using json = nlohmann::json;

class MsgSingle {
 public:
  using Q = Queue<std::string>;

  MsgSingle(const std::vector<std::string> &labels) { 
    for (const auto &label : labels) {
      msg_q_[label].reset(new Q);
    }
  }

  void add(const std::string &label, const std::string &msg) {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    it->second->push(msg);
  }

  void retrieve(const std::string &label, std::string *msg) {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    it->second->pop(msg);
  }

  template< class Rep, class Period >
  bool retrieve(const std::string &label, std::string *msg, 
      const std::chrono::duration<Rep, Period>& t) {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    return it->second->pop(msg, t);
  }

  void parseAdd(const std::string &s) {
    json j = json::parse(s);
    for (const auto &p : msg_q_) {
      const auto &label = p.first;

      if (j.find(label) == j.end()) continue;
      for (const auto &jj : j[label]) {
        p.second->push(jj);
      }
    }
  }

  std::string dumpClear() {
    json j;
    for (const auto &p : msg_q_) {
      const auto &label = p.first;
      std::string msg;
      while (p.second->pop(&msg, std::chrono::milliseconds(0))) {
        j[label].push_back(msg);
      }
    }
    
    return j.dump();
  }

 private:
  std::unordered_map<std::string, std::unique_ptr<Q>> msg_q_; 
};

class MsgQ {
 public:
  MsgQ() : rng_(time(NULL)) {}

  MsgSingle &addQ(const std::string &identity, const std::vector<std::string> &labels) {
    std::lock_guard<std::mutex> locker(mutex_);
    auto info = msg_qs_.insert(make_pair(identity, nullptr));
    if (! info.second) {
      std::cout << "addQ: identity " << identity << " has already been added!" << std::endl;
      assert(false);
    }

    for (const auto &label : labels) {
      label2identities_[label].push_back(identity);
    }

    info.first->second.reset(new MsgSingle(labels));
    return *info.first->second;
  }

  MsgSingle &operator[](const std::string &identity) {
    std::lock_guard<std::mutex> locker(mutex_);
    auto it = msg_qs_.find(identity);
    if (it == msg_qs_.end()) {
      std::cout << "MsgSingle: Cannot find " << identity << std::endl;
      assert(false);
    }
    return *it->second;
  }

  std::string sample(const std::string &label) {
    std::string id;
    auto f = [this, &id](const std::vector<std::string> &identities) {
        int idx = rng_() % identities.size();
        id = identities[idx];
        return true;
    };

    _call_when_label_available(label, f);
    return id;
  }

  std::string recvFromLabel(const std::string &label, std::string *msg) {
    std::string id;
    auto f = [this, &id, &label, msg](const std::vector<std::string> &identities) {
      std::vector<int> indices(identities.size());
      for (size_t i = 0; i < identities.size(); i++) indices[i] = i;
      std::shuffle(indices.begin(), indices.end(), rng_);

      for (int idx : indices) {
        const auto &idd = identities[idx];
        if (msg_qs_[idd]->retrieve(label, msg, std::chrono::milliseconds(0))) {
          id = idd;
          return true;
        } 
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      return false;
    };

    _call_when_label_available(label, f);
    return id;
  }

 private:
  mutable std::mutex mutex_;
  mutable std::mt19937 rng_;

  std::unordered_map<std::string, std::vector<std::string>> label2identities_;
  std::unordered_map<std::string, std::unique_ptr<MsgSingle>> msg_qs_;

  void _call_when_label_available(const std::string &label, std::function<bool (const std::vector<std::string> &)> f) {
    while (true) {
      std::vector<std::string> identities;
      {
        std::lock_guard<std::mutex> locker(mutex_);
        auto it = label2identities_.find(label);
        if (it != label2identities_.end()) identities = it->second;
      }

      if (identities.empty()) {
        // std::cout << "No identities with label = " << label << " yet, waiting ..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
      } else {
        if (f(identities)) break;
      }
    }
  }
};

class Interface {
 public:
  Interface() {} 

  // Send message to a given identity. 
  void send(const std::string &label, const std::string &msg, const std::string& identity) {
    send_q_[identity].add(label, msg);
  }

  // Send message to a random identity, wait if there is no identity yet, 
  // and return the identity once the message is sent.  
  void sendToEligible(const std::string &label, const std::string &msg, std::string *identity) {
    *identity = send_q_.sample(label);
    send_q_[*identity].add(label, msg);
  }

  // Get message from a given identity. Block if no message. 
  void recv(const std::string &label, std::string *msg, const std::string& identity) {
    recv_q_[identity].retrieve(label, msg);
  }

  // Get message from any identity. Block if no message. 
  void recvFromEligible(const std::string &label, std::string *msg, std::string *identity) {
    *identity = recv_q_.recvFromLabel(label, msg);
  }

 protected:
  MsgQ send_q_;
  MsgQ recv_q_;
};

} // namespace remote
} // namespace elf
