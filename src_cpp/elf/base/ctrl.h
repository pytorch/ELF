/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>

#include <tbb/concurrent_hash_map.h>

#include "elf/concurrency/Counter.h"
#include "elf/concurrency/TBBHashers.h"

namespace elf {

struct Addr {
  // Should construct an invalid thread id
  std::thread::id id;
  std::string label;

  bool matchPrefix(const std::string& prefix) const {
    if (label.size() < prefix.size()) {
      return false;
    }
    return label.substr(0, prefix.size()) == prefix;
  }

  friend bool operator==(const Addr& a1, const Addr& a2) {
    return a1.id == a2.id && a1.label == a2.label;
  }

  friend bool operator!=(const Addr& a1, const Addr& a2) {
    return !(a1 == a2);
  }
};

class CtrlFuncs {
 public:
  template <typename T>
  using RecvCB_T = std::function<bool(const Addr& info, T& msg)>;

  // For processor
  template <typename T>
  void RegCallback(RecvCB_T<T> cb) {
    typename FuncMap::accessor elem;
    bool uninitialized = funcMap_.insert(elem, std::type_index(typeid(T)));
    if (uninitialized) {
      elem->second.reset(new _CallbackT<T>(cb));
    }
  }

  template <typename T>
  RecvCB_T<T> getCallback() const {
    // Readonly, no lock.
    const _CallbackBase* p;
    {
      typename FuncMap::const_accessor elem;
      bool found = funcMap_.find(elem, std::type_index(typeid(T)));
      assert(found);
      p = elem->second.get();
    }
    assert(p != nullptr);

    const _CallbackT<T>* cb_wrapper = dynamic_cast<const _CallbackT<T>*>(p);
    assert(cb_wrapper != nullptr);

    return cb_wrapper->func;
  }

 private:
  struct _CallbackBase {
    virtual ~_CallbackBase() = default;
  };

  template <typename T>
  struct _CallbackT : public _CallbackBase {
   public:
    RecvCB_T<T> func;
    _CallbackT(RecvCB_T<T> func) : func(func) {}
  };

  using FuncMap =
      tbb::concurrent_hash_map<std::type_index, std::unique_ptr<_CallbackBase>>;
  FuncMap funcMap_;
};

struct _MailboxQueueBase {
  virtual ~_MailboxQueueBase() = default;
};

template <typename Q>
struct _MailboxQueue : public _MailboxQueueBase {
  Q q;
};

using MailboxMap =
    std::unordered_map<std::type_index, std::unique_ptr<_MailboxQueueBase>>;

template <typename... Queues>
void add_mailbox(MailboxMap& mailbox);

template <typename Q, typename... Queues>
inline void add_mailbox_helper(MailboxMap& mailbox) {
  mailbox[std::type_index(typeid(typename Q::value_type))].reset(
      new _MailboxQueue<Q>());
  add_mailbox<Queues...>(mailbox);
}

template <typename... Queues>
inline void add_mailbox(MailboxMap& mailbox) {
  add_mailbox_helper<Queues...>(mailbox);
}

template <>
inline void add_mailbox(MailboxMap&) {}

template <template <typename> class Queue>
struct _ThreadInfoT {
 public:
  using Id = std::thread::id;

  const Addr& reg(Id id, const std::string& label) {
    addr_.id = id;
    addr_.label = label;
    return addr_;
  }

  template <typename... RecvTs>
  void addMailbox() {
    // Make sure reg is called.
    assert(addr_.id != std::thread::id());
    add_mailbox<Queue<RecvTs>...>(mailbox_);
  }

  const Addr& info() const {
    return addr_;
  }

  template <typename R>
  Queue<R>* getMailboxQueue() {
    auto it = mailbox_.find(std::type_index(typeid(R)));
    if (it == mailbox_.end())
      return nullptr;

    auto* mailbox = dynamic_cast<_MailboxQueue<Queue<R>>*>(it->second.get());
    assert(mailbox != nullptr);

    return &mailbox->q;
  }

 private:
  Addr addr_;
  MailboxMap mailbox_;
};

template <template <typename> class Queue>
class ThreadInfosT {
 public:
  using Id = std::thread::id;
  using _ThreadInfo = _ThreadInfoT<Queue>;

  const Addr& reg(Id id, std::string label = "") {
    typename ThreadInfoMap::accessor elem;
    bool uninitialized = threadInfoMap_.insert(elem, id);
    if (uninitialized) {
      elem->second.reset(new _ThreadInfo());
    }
    const Addr& addr = elem->second->reg(id, label);

    if (label != "") {
      typename ThreadStrMap::accessor elem2;
      bool uninitialized = threadStrMap_.insert(elem2, label);
      assert(uninitialized);
      elem2->second = id;
    }
    return addr;
  }

  template <typename... MailboxTs>
  void addMailbox(Id id) {
    _th_info(id).template addMailbox<MailboxTs...>();
  }

  bool isRegistered(Id id) const {
    typename ThreadInfoMap::const_accessor elem;
    return threadInfoMap_.find(elem, id);
  }

  bool isRegistered(const std::string label) const {
    typename ThreadStrMap::const_accessor elem;
    return threadStrMap_.find(elem, label);
  }

  const Addr& getAddr(Id id) const {
    return _th_info(id).info();
  }

  template <typename R>
  void waitMail(Id id, R* r) {
    Queue<R>* q = _th_info(id).template getMailboxQueue<R>();
    assert(q != nullptr);
    q->pop(r);
  }

  template <typename R>
  bool peekMail(Id id, R* r, int timeout_usec) {
    Queue<R>* q = _th_info(id).template getMailboxQueue<R>();
    assert(q != nullptr);
    return q->pop(r, std::chrono::microseconds(timeout_usec));
  }

  template <typename R>
  void sendMail(Id id, const R& r) {
    Queue<R>* q = _th_info(id).template getMailboxQueue<R>();
    assert(q != nullptr);
    q->push(r);
  }

  template <typename R>
  void sendMail(const std::string& label, const R& r) {
    sendMail<R>(_th_label2id(label), r);
  }

  std::vector<Addr> filterPrefix(const std::string& prefix) {
    std::vector<Addr> senders;
    for (auto& elem : threadInfoMap_.range()) {
      auto& threadInfo = *(elem.second);
      if (threadInfo.info().matchPrefix(prefix)) {
        senders.push_back(threadInfo.info());
      }
    }
    return senders;
  }

 private:
  _ThreadInfo* _th_info_impl(Id id) const {
    typename ThreadInfoMap::accessor elem;
    bool found = threadInfoMap_.find(elem, id);
    assert(found);
    _ThreadInfo* res = elem->second.get();
    assert(res != nullptr);
    return res;
  }

  std::thread::id _th_label2id(const std::string& label) const {
    typename ThreadStrMap::accessor elem;
    // std::cout << "looking for label: " << label << std::endl;
    bool found = threadStrMap_.find(elem, label);
    assert(found);
    return elem->second;
  }

  _ThreadInfo& _th_info(Id id) {
    return *_th_info_impl(id);
  }

  const _ThreadInfo& _th_info(Id id) const {
    return *_th_info_impl(id);
  }

  using ThreadInfoMap =
      tbb::concurrent_hash_map<std::thread::id, std::unique_ptr<_ThreadInfo>>;
  using ThreadStrMap = tbb::concurrent_hash_map<std::string, std::thread::id>;

  ThreadInfoMap threadInfoMap_;
  ThreadStrMap threadStrMap_;
};

template <template <typename> class Queue>
class CtrlT {
 public:
  using Ctrl = CtrlT<Queue>;

  // Sender side.
  const Addr& reg(std::string label = "") {
    return threads_.reg(std::this_thread::get_id(), label);
  }

  bool isRegistered() const {
    return threads_.isRegistered(std::this_thread::get_id());
  }

  bool isRegistered(std::string label) const {
    return threads_.isRegistered(label);
  }

  template <typename... RecvTs>
  void addMailbox() {
    return threads_.template addMailbox<RecvTs...>(std::this_thread::get_id());
  }

  // Only works if the id is registered.
  const Addr& getAddr() const {
    return threads_.getAddr(std::this_thread::get_id());
  }

  // Call the registered function via the same thread/another thread.
  template <typename T>
  bool call(T& msg) {
    auto cb = callbacks_.template getCallback<T>();
    assert(cb != nullptr);
    const auto& addr = threads_.getAddr(std::this_thread::get_id());
    return cb(addr, msg);
  }

  template <typename R>
  void waitMail(R* r) {
    auto id = std::this_thread::get_id();
    threads_.template waitMail<R>(id, r);
  }

  template <typename R>
  bool peekMail(R* r, int timeout_usec) {
    auto id = std::this_thread::get_id();
    return threads_.template peekMail<R>(id, r, timeout_usec);
  }

  void waitRegs(int num_senders) {
    threads_.waitRegs(num_senders);
  }

  //
  template <typename T>
  void RegCallback(CtrlFuncs::RecvCB_T<T> cb) {
    callbacks_.template RegCallback<T>(cb);
  }

  template <typename R>
  void sendMail(const Addr& addr, const R& r) {
    threads_.template sendMail<R>(addr.id, r);
  }

  template <typename R>
  void sendMail(const std::string& label, const R& r) {
    threads_.template sendMail<R>(label, r);
  }

  std::vector<Addr> filterPrefix(const std::string& prefix) {
    return threads_.template filterPrefix(prefix);
  }

 protected:
  CtrlFuncs callbacks_;
  ThreadInfosT<Queue> threads_;
};

template <template <typename> class Queue>
class ThreadedCtrlBaseT {
 public:
  using Ctrl = CtrlT<Queue>;

  ThreadedCtrlBaseT(Ctrl& ctrl, int time_millisec)
      : ctrl_(ctrl), time_millisec_(time_millisec), done_(false) {}

  template <typename T>
  void sendToThread(const T& msg) {
    ctrl_.sendMail(addr_, msg);
  }

  virtual ~ThreadedCtrlBaseT() {
    done_ = true;
    // std::cout << "ThreadedCtrlBase: Ending thread.." << std::endl;
    if (thread_ != nullptr) {
      thread_->join();
    }

    // std::cout << "ThreadedCtrlBase: thread ended.." << std::endl;
  }

 protected:
  Ctrl& ctrl_;
  int time_millisec_;

  elf::concurrency::Switch startedSwitch_;

  Addr addr_;
  std::atomic_bool done_;
  std::unique_ptr<std::thread> thread_;

  // Sample usage:
  //  start<Received Types>
  //  ctrl_.sendMail(label, content);
  //  ctrl_.waitMail/peekMail.
  virtual void on_thread() = 0;
  virtual void before_loop() {}

  template <typename... Ts>
  void start(std::string label = "") {
    done_ = false;
    thread_.reset(new std::thread([this, label]() {
      addr_ = ctrl_.reg(label);
      ctrl_.template addMailbox<Ts...>();
      startedSwitch_.set(true);
      before_loop();

      while (!done_.load()) {
        on_thread();
        std::this_thread::sleep_for(std::chrono::milliseconds(time_millisec_));
      }
    }));

    // Here we need to wait until addr_ is valid..
    startedSwitch_.waitUntilTrue();
    startedSwitch_.reset();
  }
};

} // namespace elf

namespace std {

template <>
struct hash<elf::Addr> {
  typedef elf::Addr argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& s) const noexcept {
    result_type const h1(std::hash<std::thread::id>{}(s.id));
    result_type const h2(std::hash<std::string>{}(s.label));
    // [TODO] Not a good combination..we might need to try something different.
    return h1 ^ (h2 << 1);
  }
};

} // namespace std
