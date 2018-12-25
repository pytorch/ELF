#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <iostream>
#include <atomic>

#include "client_manager_def.h"
#include "record.h"
#include "elf/options/reflection_option.h"
#include "elf/utils/utils.h"

#include "options.h"

namespace elf {

namespace cs {

class ClientManager;

struct ClientInfo {
 public:
  enum ClientChange { ALIVE2DEAD, DEAD2ALIVE, ALIVE, DEAD };

  struct State {
    mutable std::mutex mutex_;
    ThreadState last_state_;
    uint64_t last_state_update_ = 0;
    const ClientManager& mgr_;

    State(const ClientManager& mgr);
    // bool CompareGame(const ModelPair& p) const;
    bool StateUpdate(const ThreadState& ts);
  };

  ClientInfo(
      const ClientManager& mgr,
      const std::string& id,
      int num_threads,
      int max_delay_sec);

  const ClientManager& getManager() const {
    return mgr_;
  }

  const std::string& id() const {
    return identity_;
  }
  int seq() const {
    return seq_.load();
  }
  bool justAllocated() const {
    return seq_ == 0;
  }
  void incSeq() {
    seq_++;
  }

  ClientType type() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return type_;
  }

  void set_type(ClientType t) {
    std::lock_guard<std::mutex> lock(mutex_);
    type_ = t;
  }

  bool IsActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
  }

  bool IsStuck(uint64_t curr_timestamp, uint64_t* delay = nullptr) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto last_delay = curr_timestamp - last_update_;
    if (delay != nullptr)
      *delay = last_delay;
    return last_delay >= max_delay_sec_;
  }

  void stateUpdate(const ThreadState& ts);

  ClientChange updateActive();

  const State& threads(int thread_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    assert(thread_id >= 0 && thread_id < (int)threads_.size());
    return *threads_[thread_id];
  }

 private:
  mutable std::mutex mutex_;
  const ClientManager& mgr_;
  const std::string identity_;
  ClientType type_ = -1;

  uint64_t max_delay_sec_ = 300;

  std::atomic<int64_t> seq_;

  bool active_ = true;
  uint64_t last_update_ = 0;
  std::vector<std::unique_ptr<State>> threads_;
};


class ClientManager {
 public:
  ClientManager(
        const ClientManagerOptions &options, 
        std::function<uint64_t()> timer = elf_utils::sec_since_epoch_from_now)
      : options_(options), timer_(timer) {
    assert(timer_ != nullptr);
    assert(options_.client_type_ratios.size() == options_.client_type_limits.size());
    num_clients_.resize(options_.client_type_ratios.size(), 0);
  }

  void setClientTypeRatio(const std::vector<float> &ratio) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.client_type_ratios = ratio;
  }

  int getExpectedNum(ClientType t) const {
    assert(t >= 0 && t < (int)num_clients_.size());
    return std::min(options_.client_type_limits[t], 
        static_cast<int>(options_.client_type_ratios[t] * options_.expected_num_clients + 0.5));
  }

  const ClientInfo& updateStates(
      const std::string& identity,
      const std::unordered_map<int, ThreadState>& states) {
    std::lock_guard<std::mutex> lock(mutex_);
    ClientInfo& info = _getClient(identity);

    // Print out the stats.
    /*
    std::cout << "State update[" << rs.identity << "][" << elf_utils::now() <<
    "]"; for (const auto& s : rs.states) { std::cout << s.second.info() << ", ";
    }
    std::cout << std::endl;
    */

    for (const auto& s : states) {
      info.stateUpdate(s.second);
    }

    // A client is considered dead after 20 min.
    updateClients();
    return info;
  }

  const ClientInfo* getClientC(const std::string& identity) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(identity);
    if (it != clients_.end()) {
      return it->second.get();
    } else {
      return nullptr;
    }
  }

  ClientInfo& getClient(const std::string& identity) {
    std::lock_guard<std::mutex> lock(mutex_);
    return _getClient(identity);
  }

  uint64_t getCurrTimeStamp() const {
    return timer_();
  }

  std::string info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << options_.info() << std::endl;

    if (n_ > 0) {
      for (ClientType t = 0; t < (int)num_clients_.size(); t++) {
        float curr_ratio = static_cast<float>(num_clients_[t]) / n_;
        ss << t << ": " << curr_ratio << "/" << num_clients_[t] << ",";
      }
    }
    return ss.str();
  }

 private:
  ClientManagerOptions options_;
  std::function<uint64_t()> timer_ = nullptr;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<ClientInfo>> clients_;

  std::vector<int> num_clients_;
  int n_ = 0;

  ClientType alloc_type() {
    // Always allocate first type, when there is no clients left.
    if (n_ == 0) return 0;

    std::vector<ClientType> pri0, pri1;

    for (ClientType t = 0; t < (int)num_clients_.size(); t++) {
      if (num_clients_[t] < options_.client_type_limits[t]) {
        pri1.push_back(t);
      }

      float curr_ratio = static_cast<float>(num_clients_[t]) / n_;
      if (curr_ratio < options_.client_type_ratios[t]) {
        pri0.push_back(t);
      }
    }

    ClientType t_choice = CLIENT_INVALID;
    if (! pri0.empty()) t_choice = pri0[0];
    else if (! pri1.empty()) t_choice = pri1[0];

    assert(t_choice != CLIENT_INVALID);

    num_clients_[t_choice] ++;
    n_ ++;
    return t_choice;
  }

  void dealloc_type(ClientType t) {
    assert(t >= 0 && t < (int)num_clients_.size());
    num_clients_[t] --;
    n_ --;
  }

  void updateClients() {
    std::vector<std::string> newly_dead;
    std::vector<std::string> newly_alive;

    for (auto& p : clients_) {
      auto& c = *p.second;
      auto status = c.updateActive();
      if (status == ClientInfo::ALIVE2DEAD) {
        newly_dead.push_back(p.first);
        dealloc_type(c.type());
      } else if (status == ClientInfo::DEAD2ALIVE) {
        newly_alive.push_back(p.first);
        c.set_type(alloc_type());
      }
    }

    if (!newly_dead.empty() || !newly_alive.empty()) {
      std::cout << getCurrTimeStamp()
                << " Client newly dead: " << newly_dead.size()
                << ", newly alive: " << newly_alive.size() << ", " << options_.info()
                << std::endl;
      for (const auto& s : newly_dead) {
        std::cout << "Newly dead: " << s << std::endl;
      }
      for (const auto& s : newly_alive) {
        std::cout << "Newly alive: " << s << std::endl;
      }
    }
  }

  ClientInfo& _getClient(const std::string& identity) {
    auto it = clients_.find(identity);
    if (it != clients_.end())
      return *it->second;

    auto& e = clients_[identity];
    e.reset(new ClientInfo(
        *this, identity, options_.max_num_threads, options_.client_max_delay_sec));
    e->set_type(alloc_type());
    return *e;
  }
};

}  // namespace cs

}  // namespace elf
