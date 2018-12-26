#pragma once

#include "elf/concurrency/ConcurrentQueue.h"
#include "elf/utils/utils.h"

#include <set>

namespace elf {
namespace remote {

static constexpr int kPortPerClient = 2;
static constexpr int kPortPerServer = 4;

template <typename T>
using Queue = elf::concurrency::ConcurrentQueueMoodyCamelNoCheck<T>;

inline std::string timestr() {
  return std::to_string(elf_utils::msec_since_epoch_from_now());
}

inline std::vector<int> getShuffled(size_t n, std::mt19937 &rng) {
  std::vector<int> indices(n);
  for (size_t i = 0; i < n; i++) indices[i] = i;
  std::shuffle(indices.begin(), indices.end(), rng);

  return indices;
}

using json = nlohmann::json;

class SendSingleInterface {
 public:
  virtual void add(const std::string &label, const std::string &msg) {
    (void)label;
    (void)msg;
  }

  virtual std::string dumpClear(int *) = 0;
};

class RecvSingleInterface {
 public:
  virtual void retrieve(const std::string &label, std::string *msg) {
    (void)label;
    (void)msg;
  }
  virtual bool retrieveNow(const std::string &label, std::string *msg) {
    (void)label;
    (void)msg;
    return false;
  }
  virtual bool retrieveAnyNow(std::string *label, std::string *msg) {
    (void)label;
    (void)msg;
    return false;
  }
  virtual void parseAdd(const std::string &s) = 0;
};

class SingleQBase {
 public:
  using Q = Queue<std::string>;

  SingleQBase(const std::vector<std::string> &labels)
    : labels_(labels) {
    for (const auto &label : labels) {
      msg_q_[label].reset(new Q);
    }
  }

 protected:
  std::unordered_map<std::string, std::unique_ptr<Q>> msg_q_;
  std::vector<std::string> labels_;
};

class SendSingle : public SendSingleInterface, public SingleQBase {
 public:
  SendSingle(const std::vector<std::string> &labels)
    : SingleQBase(labels) {}

  void add(const std::string &label, const std::string &msg) override {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    it->second->push(msg);
  }

  std::string dumpClear(int *num_record) override {
    json j;

    *num_record = 0;
    for (const auto &p : msg_q_) {
      const auto &label = p.first;
      std::string msg;
      while (p.second->pop(&msg, std::chrono::milliseconds(0))) {
        j[label].push_back(msg);
        (*num_record) ++;
      }
    }
    return j.dump();
  }
};

class RecvSingle : public RecvSingleInterface, public SingleQBase {
 public:
  RecvSingle(const std::vector<std::string> &labels)
    : SingleQBase(labels) {}

  void retrieve(const std::string &label, std::string *msg) override {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    it->second->pop(msg);
  }

  bool retrieveNow(const std::string &label, std::string *msg) override {
    auto it = msg_q_.find(label);
    assert(it != msg_q_.end());
    return it->second->pop(msg, std::chrono::milliseconds(0));
  }

  bool retrieveAnyNow(std::string *label, std::string *msg) override {
    std::vector<int> indices = getShuffled(labels_.size(), rng_);

    for (int idx : indices) {
      auto it = msg_q_.find(labels_[idx]);
      assert(it != msg_q_.end());
      if (it->second->pop(msg, std::chrono::milliseconds(0))) {
        *label = it->first;
        return true;
      }
    }
    return false;
  }

  void parseAdd(const std::string &s) override {
    json j = json::parse(s);
    for (const auto &p : msg_q_) {
      const auto &label = p.first;

      if (j.find(label) == j.end()) continue;
      for (const auto &jj : j[label]) {
        p.second->push(jj);
      }
    }
  }
 private:
  std::mt19937 rng_;
};

template <typename T>
class QBase {
 public:
  using value_type = T;
  using Ls = std::vector<std::string>;
  using Gen = std::function<std::unique_ptr<T> (const Ls &)>;

  using SafeFunc = std::function<bool (const Ls &, const std::vector<T *> &)>;
  using FilterFunc = std::function<bool (const std::string &, T *)>;

  QBase() : rng_(time(NULL)) { }
  void setGen(Gen gen) { gen_ = gen; }

  T &addQ(const std::string &identity, const Ls &labels) {
    assert(gen_ != nullptr);

    std::lock_guard<std::mutex> locker(mutex_);
    auto info = msg_qs_.insert(make_pair(identity, nullptr));
    if (! info.second) {
      std::cout << "addQ: identity " << identity << " has already been added!" << std::endl;
      elf_utils::check(false);
    }

    for (const auto &label : labels) {
      label2identities_[label].push_back(identity);
    }

    info.first->second = gen_(labels);
    return *info.first->second;
  }

  T &operator[](const std::string &identity) {
    std::lock_guard<std::mutex> locker(mutex_);
    auto it = msg_qs_.find(identity);
    if (it == msg_qs_.end()) {
      std::cout << "QBase: Cannot find \"" << identity << "\"" << std::endl;
      elf_utils::check(false);
    }
    return *it->second;
  }

  bool findFirst(FilterFunc func) {
    // TODO: better release the locker when calling the function.
    std::lock_guard<std::mutex> locker(mutex_);
    for (auto &p : msg_qs_) {
      if (func(p.first, p.second.get())) return true;
    }
    return false;
  }

  bool findFirst(const std::set<std::string> &ids, FilterFunc func) {
    // TODO: better release the locker when calling the function.
    std::lock_guard<std::mutex> locker(mutex_);
    for (const auto &id : ids) {
      auto it = msg_qs_.find(id);
      assert(it != msg_qs_.end());
      if (func(id, it->second.get())) return true;
    }
    return false;
  }

 private:
  mutable std::mutex mutex_;

  Gen gen_ = nullptr;

  std::unordered_map<std::string, Ls> label2identities_;
  std::unordered_map<std::string, std::unique_ptr<T>> msg_qs_;

 protected:
  mutable std::mt19937 rng_;

  void _call_when_label_available(const std::string &label, SafeFunc f) {
    while (true) {
      Ls identities;
      {
        std::lock_guard<std::mutex> locker(mutex_);
        auto it = label2identities_.find(label);
        if (it != label2identities_.end()) identities = it->second;
      }

      if (identities.empty()) {
        // std::cout << "No identities with label = " << label << " yet, waiting ..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
      } else {
        std::vector<T *> qs;
        {
          std::lock_guard<std::mutex> locker(mutex_);
          for (const auto &id : identities) {
            qs.push_back(msg_qs_[id].get());
          }
        }

        if (f(identities, qs)) break;
      }
    }
  }

  void _call(SafeFunc f) {
    Ls identities;
    std::vector<T *> qs;

    while (true) {
      {
        std::lock_guard<std::mutex> locker(mutex_);
        for (auto &p : msg_qs_) {
          identities.push_back(p.first);
          qs.push_back(p.second.get());
        }
      }

      if (f(identities, qs)) break;
    }
  }
};

class SendQ : public QBase<SendSingleInterface> {
 public:
  using Super = QBase<SendSingleInterface>;
  using Ls = typename Super::Ls;
  using Gen = typename Super::Gen;

  std::string sample(const std::string &label) {
    std::string id;
    auto f = [this, &id](const Ls &identities, const std::vector<SendSingleInterface *> &) {
        int idx = rng_() % identities.size();
        id = identities[idx];
        return true;
    };

    _call_when_label_available(label, f);
    return id;
  }
};

class RecvQ : public QBase<RecvSingleInterface> {
 public:
  using Super = QBase<RecvSingleInterface>;
  using Ls = typename Super::Ls;
  using Gen = typename Super::Gen;

  std::string recvFromLabel(const std::string &label, std::string *msg) {
    std::string id;
    auto f = [this, &id, &label, msg](const Ls &identities, const std::vector<RecvSingleInterface *> &qs) {
      std::vector<int> indices = getShuffled(qs.size(), rng_);

      for (int idx : indices) {
        const auto &idd = identities[idx];
        if (qs[idx]->retrieveNow(label, msg)) {
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

  std::string recvFromAll(std::string *label, std::string *msg) {
    std::string id;
    auto f = [this, &id, label, msg](const Ls &identities, const std::vector<RecvSingleInterface *> &qs) {
      std::vector<int> indices = getShuffled(identities.size(), rng_);

      for (int idx : indices) {
        const auto &idd = identities[idx];
        if (qs[idx]->retrieveAnyNow(label, msg)) {
          id = idd;
          return true;
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      return false;
    };

    _call(f);
    return id;
  }
};

class Interface {
 public:
  Interface() {
    auto gen_send = [](const SendQ::Ls &labels) { return std::make_unique<SendSingle>(labels); };
    auto gen_recv = [](const RecvQ::Ls &labels) { return std::make_unique<RecvSingle>(labels); };
    setSendGen(gen_send);
    setRecvGen(gen_recv);
  }

  void setSendGen(SendQ::Gen gen) { send_q_.setGen(gen); }
  void setRecvGen(RecvQ::Gen gen) { recv_q_.setGen(gen); }

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

  // Get message from any identity. Block if no message.
  void recvFromAll(std::string *label, std::string *msg, std::string *identity) {
    *identity = recv_q_.recvFromAll(label, msg);
  }

 protected:
  SendQ send_q_;
  RecvQ recv_q_;
};

} // namespace remote
} // namespace elf
