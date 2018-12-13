#pragma once

#include "../mcts/mcts.h"
#include "elf/ai/tree_search/mcts.h"
#include "go_state_ext.h"
#include "record.h"

struct GuardedRecords {
 public:
  GuardedRecords(const std::string& identity) : records_(identity) {}

  void feed(const GoStateExt& s) {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.addRecord(s.dumpRecord());
  }

  void updateState(const ThreadState& ts) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = elf_utils::sec_since_epoch_from_now();
    records_.updateState(ts);

    last_states_.push_back(std::make_pair(now, ts));
    if (last_states_.size() > 100) {
      last_states_.pop_front();
    }

    if (now - last_state_vis_time_ > 60) {
      std::unordered_map<int, ThreadState> states;
      std::unordered_map<int, uint64_t> timestamps;
      for (const auto& s : last_states_) {
        timestamps[s.second.thread_id] = s.first;
        states[s.second.thread_id] = s.second;
      }

      std::cout << "GuardedRecords::updateState[" << elf_utils::now() << "] "
                << visStates(states, &timestamps) << std::endl;

      last_state_vis_time_ = now;
    }
  }

  size_t size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.records.size();
  }

  std::string dumpAndClear() {
    // send data.
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "GuardedRecords::DumpAndClear[" << elf_utils::now()
              << "], #records: " << records_.records.size();

    std::cout << ", " << visStates(records_.states) << std::endl;
    std::string s = records_.dumpJsonString();
    records_.clear();
    return s;
  }

 private:
  std::mutex mutex_;
  Records records_;
  std::deque<std::pair<uint64_t, ThreadState>> last_states_;
  uint64_t last_state_vis_time_ = 0;

  static std::string visStates(
      const std::unordered_map<int, ThreadState>& states,
      const std::unordered_map<int, uint64_t>* timestamps = nullptr) {
    std::stringstream ss;
    ss << "#states: " << states.size();
    ss << "[";

    auto now = elf_utils::sec_since_epoch_from_now();
    std::vector<int> ordered;
    for (const auto& p : states) {
      ordered.push_back(p.first);
    }
    std::sort(ordered.begin(), ordered.end());

    for (const auto& th_id : ordered) {
      auto it = states.find(th_id);
      assert(it != states.end());

      ss << th_id << ":" << it->second.seq << ":" << it->second.move_idx;

      if (timestamps != nullptr) {
        auto it = timestamps->find(th_id);
        if (it != timestamps->end()) {
          uint64_t td = now - it->second;
          ss << ":" << td;
        }
        ss << ",";
      }
    }
    ss << "]  ";

    ss << elf_utils::get_gap_list(ordered);
    return ss.str();
  }
};

class GameNotifier {
 public:
  using MCTSResult = elf::ai::tree_search::MCTSResultT<Coord>;

  GameNotifier(
      const std::string& identity,
      const GameOptionsSelfPlay& options,
      elf::GameClientInterface* client)
      : records_(identity), options_(options), client_(client) {}

  std::string DumpRecords(int* sz) {
    *sz = records_.size();
    return records_.dumpAndClear();
  }

  void OnGameEnd(const GoStateExt& s) {
    // tell python / remote
    records_.feed(s);

    game_stats_.resetRankingIfNeeded(options_.num_reset_ranking);
    game_stats_.feedWinRate(s.state().getFinalValue());
    // game_stats_.feedSgf(s.dumpSgf(""));

    // Report winrate (so that Python side could know).
    auto binder = client_->getBinder();
    elf::FuncsWithState funcs =
        binder.BindStateToFunctions({end_target_}, &s);
    client_->sendWait({end_target_}, &funcs);
  }

  void OnStateUpdate(const ThreadState& state) {
    // Update current state.
    records_.updateState(state);
  }

  void OnMCTSResult(Coord c, const MCTSResult& result) {
    // Check the ranking of selected move.
    auto move_rank =
        result.getRank(c, elf::ai::tree_search::MCTSResultT<Coord>::PRIOR);
    game_stats_.feedMoveRanking(move_rank.first);
  }

  GameStats& getGameStats() {
    return game_stats_;
  }

 private:
  GameStats game_stats_;
  GuardedRecords records_;
  const GameOptionsSelfPlay options_;
  elf::GameClientInterface* client_ = nullptr;
  const std::string end_target_ = "game_end";
};
