#pragma once
#include "game_ctrl.h"

#include "elf/distri/game_interface.h"

class ServerWrapper : public elf::sc::ServerInterface {
 public:
  ServerWrapper() {
    logger_->info(
        "Finished initializing replay_buffer {}", replay_buffer_->info());
    threaded_ctrl_.reset(
        new ThreadedCtrl(ctrl_, client, replay_buffer_.get(), options));
  }

  void onStart() override { 
    // Call by shared_rw thread or any thread that will call OnReceive.
    ctrl_.reg("train_ctrl");
    ctrl_.addMailbox<int>();
    threaded_ctrl_->Start();
  }
  
  void onReceive(const Records &rs, const ClientInfo& info, replay_buffer) override {
    if (rs.identity.size() == 0) {
      // No identity -> offline data.
      for (auto& r : rs.records) {
        r.offline = true;
      }
    }

    std::vector<FeedResult> selfplay_res =
        threaded_ctrl_->onSelfplayGames(rs.records);

    elf::shared::InsertInfo insert_info;
    for (size_t i = 0; i < rs.records.size(); ++i) {
      if (selfplay_res[i] == FeedResult::FEEDED ||
          selfplay_res[i] == FeedResult::VERSION_MISMATCH) {
        const Record& r = rs.records[i];

        bool black_win = r.result.reward > 0;
        insert_info +=
            replay_buffer_->InsertWithParity(Record(r), &rng_, black_win);
        selfplay_record_.feed(r);
        selfplay_record_.saveAndClean(1000);
      }
    }

    std::vector<FeedResult> eval_res =
        threaded_ctrl_->onEvalGames(info, rs.records);
    threaded_ctrl_->checkNewModel(client_mgr_.get());

    recv_count_++;
    if (recv_count_ % 1000 == 0) {
      int valid_selfplay = 0, valid_eval = 0;
      for (size_t i = 0; i < rs.records.size(); ++i) {
        if (selfplay_res[i] == FeedResult::FEEDED)
          valid_selfplay++;
        if (eval_res[i] == FeedResult::FEEDED)
          valid_eval++;
      }

      std::cout << "TrainCtrl: Receive data[" << recv_count_ << "] from "
                << rs.identity << ", #state_update: " << rs.states.size()
                << ", #records: " << rs.records.size()
                << ", #valid_selfplay: " << valid_selfplay
                << ", #valid_eval: " << valid_eval << std::endl;
    }
    return insert_info;
  }

  void fillInRequest(const ClientInfo &info, MsgRequestSeq *request) override {
    threaded_ctrl_->fillInRequest(info, &request->request);
  }

  ThreadedCtrl* getThreadedCtrl() {
    return threaded_ctrl_.get();
  }

  bool setEvalMode(int64_t new_ver, int64_t old_ver) {
    //std::cout << "Setting eval mode: new: " << new_ver << ", old: " << old_ver
    //          << std::endl;
    client_mgr_->setSelfplayOnlyRatio(0.0);
    threaded_ctrl_->setEvalMode(new_ver, old_ver);
    return true;
  }

 private:
  Ctrl ctrl_;
  std::unique_ptr<ThreadedCtrl> threaded_ctrl_;

  int recv_count_ = 0;
  std::mt19937 rng_;

  // SelfCtrl has its own record buffer to save EVERY game it has received.
  RecordBufferSimple selfplay_record_;

  std::shared_ptr<spdlog::logger> logger_;
};
