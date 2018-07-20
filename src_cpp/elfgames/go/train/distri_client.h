#pragma once

#include <chrono>
#include "../common/dispatcher_callback.h"
#include "distri_base.h"

using ThreadedCtrlBase = elf::ThreadedCtrlBase;

class ThreadedWriterCtrl : public ThreadedCtrlBase {
 public:
  ThreadedWriterCtrl(
      Ctrl& ctrl,
      const ContextOptions& contextOptions,
      const GameOptions& options)
      : ThreadedCtrlBase(ctrl, 0),
        logger_(elf::logging::getLogger("ThreadedWriterCtrl-", "")) {
    elf::shared::Options netOptions = getNetOptions(contextOptions, options);
    writer_.reset(new elf::shared::Writer(netOptions));
    auto currTimestamp = time(NULL);
    logger_->info(
        "Writer info: {}, send ctrl with timestamp {} ",
        writer_->info(),
        currTimestamp);
    writer_->Ctrl(std::to_string(currTimestamp));

    start<>();
  }

  std::string identity() const {
    return writer_->identity();
  }

 protected:
  std::unique_ptr<elf::shared::Writer> writer_;
  int64_t seq_ = 0;
  uint64_t ts_since_last_sent_ = elf_utils::sec_since_epoch_from_now();
  std::shared_ptr<spdlog::logger> logger_;

  static const uint64_t kMaxSecSinceLastSent = 900;

  void on_thread() {
    std::string smsg;

    uint64_t now = elf_utils::sec_since_epoch_from_now();

    // Will block..
    if (!writer_->getReplyNoblock(&smsg)) {
      std::cout << elf_utils::now() << ", WriterCtrl: no message, seq=" << seq_
                << ", since_last_sec=" << now - ts_since_last_sent_;

      // 900s = 15min
      if (now - ts_since_last_sent_ < kMaxSecSinceLastSent) {
        std::cout << ", sleep for 10 sec .. " << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
      } else {
        std::cout << ", no reply for too long (" << now - ts_since_last_sent_
                  << '>' << kMaxSecSinceLastSent << " sec), resending"
                  << std::endl;
        getContentAndSend(seq_, false);
      }
      return;
    }

    std::cout << elf_utils::now()
              << " In reply func: Message got. since_last_sec="
              << now - ts_since_last_sent_ << ", seq=" << seq_ << ", " << smsg
              << std::endl;

    json j = json::parse(smsg);
    MsgRequestSeq msg = MsgRequestSeq::createFromJson(j);

    ctrl_.sendMail("dispatcher", msg.request);

    getContentAndSend(msg.seq, msg.request.vers.wait());
  }

  void getContentAndSend(int64_t msg_seq, bool iswait) {
    if (msg_seq != seq_) {
      std::cout << "Warning! The sequence number [" << msg_seq
                << "] in the msg is different from " << seq_ << std::endl;
    }

    std::pair<int, std::string> content;
    ctrl_.call(content);

    if (iswait) {
      std::this_thread::sleep_for(std::chrono::seconds(30));
    } else {
      if (content.first == 0)
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    /*
    std::cout << "Sending state update[" << records_.identity << "][" <<
    elf_utils::now() << "]"; for (const auto& s : records_.states) { std::cout
    << s.second.info() << ", ";
    }
    std::cout << std::endl;
    */
    writer_->Insert(content.second);
    seq_ = msg_seq + 1;
    ts_since_last_sent_ = elf_utils::sec_since_epoch_from_now();
  }
};

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

class GameNotifier : public GameNotifierBase {
 public:
  GameNotifier(
      Ctrl& ctrl,
      const std::string& identity,
      const GameOptions& options,
      elf::GameClient* client)
      : ctrl_(ctrl), records_(identity), options_(options), client_(client) {
    using std::placeholders::_1;
    using std::placeholders::_2;

    ctrl.RegCallback<std::pair<int, std::string>>(
        std::bind(&GameNotifier::dump_records, this, _1, _2));
  }

  void OnGameEnd(const GoStateExt& s) override {
    // tell python / remote
    records_.feed(s);

    game_stats_.resetRankingIfNeeded(options_.num_reset_ranking);
    game_stats_.feedWinRate(s.state().getFinalValue());
    // game_stats_.feedSgf(s.dumpSgf(""));

    // Report winrate (so that Python side could know).
    elf::FuncsWithState funcs =
        client_->BindStateToFunctions({end_target_}, &s);
    client_->sendWait({end_target_}, &funcs);
  }

  void OnStateUpdate(const ThreadState& state) override {
    // Update current state.
    records_.updateState(state);
  }

  void OnMCTSResult(Coord c, const GameNotifierBase::MCTSResult& result)
      override {
    // Check the ranking of selected move.
    auto move_rank =
        result.getRank(c, elf::ai::tree_search::MCTSResultT<Coord>::PRIOR);
    game_stats_.feedMoveRanking(move_rank.first);
  }

  GameStats& getGameStats() {
    return game_stats_;
  }

 private:
  Ctrl& ctrl_;
  GameStats game_stats_;
  GuardedRecords records_;
  const GameOptions options_;
  elf::GameClient* client_ = nullptr;
  const std::string end_target_ = "game_end";

  bool dump_records(const Addr&, std::pair<int, std::string>& data) {
    data.first = records_.size();
    data.second = records_.dumpAndClear();
    return true;
  }
};

class Client {
 public:
  Client(
      const ContextOptions& contextOptions,
      const GameOptions& options,
      elf::GameClient* client)
      : contextOptions_(contextOptions),
        options_(options),
        logger_(elf::logging::getLogger("Client-", "")) {
    dispatcher_.reset(new ThreadedDispatcher(ctrl_, contextOptions.num_games));
    dispatcher_callback_.reset(
        new DispatcherCallback(dispatcher_.get(), client));

    if (options_.mode == "selfplay") {
      writer_ctrl_.reset(
          new ThreadedWriterCtrl(ctrl_, contextOptions, options));
      game_notifier_.reset(
          new GameNotifier(ctrl_, writer_ctrl_->identity(), options, client));
    } else if (options_.mode == "online") {
    } else {
      throw std::range_error("options.mode not recognized! " + options_.mode);
    }
  }

  ~Client() {
    game_notifier_.reset(nullptr);
    dispatcher_.reset(nullptr);
    writer_ctrl_.reset(nullptr);
  }

  ThreadedDispatcher* getDispatcher() {
    return dispatcher_.get();
  }

  GameNotifier* getNotifier() {
    return game_notifier_.get();
  }

  const GameStats& getGameStats() const {
    assert(game_notifier_ != nullptr);
    return game_notifier_->getGameStats();
  }

  // Used in client side.
  void setRequest(
      int64_t black_ver,
      int64_t white_ver,
      float thres,
      int numThreads = -1) {
    MsgRequest request;
    request.vers.black_ver = black_ver;
    request.vers.white_ver = white_ver;
    request.vers.mcts_opt = contextOptions_.mcts_options;
    request.client_ctrl.black_resign_thres = thres;
    request.client_ctrl.white_resign_thres = thres;
    request.client_ctrl.num_game_thread_used = numThreads;
    dispatcher_->sendToThread(request);
  }

 private:
  Ctrl ctrl_;

  /// ZMQClient
  std::unique_ptr<ThreadedDispatcher> dispatcher_;
  std::unique_ptr<ThreadedWriterCtrl> writer_ctrl_;

  std::unique_ptr<DispatcherCallback> dispatcher_callback_;
  std::unique_ptr<GameNotifier> game_notifier_;

  const ContextOptions contextOptions_;
  const GameOptions options_;

  std::shared_ptr<spdlog::logger> logger_;
};
