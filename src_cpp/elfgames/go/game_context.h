/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>
#include <memory>
#include <vector>

// TODO: Figure out how to remove this (ssengupta@fb)
#include <time.h>

#include "base/board_feature.h"
#include "data_loader.h"
#include "elf/base/context.h"
#include "elf/legacy/python_options_utils_cpp.h"
#include "game_selfplay.h"
#include "game_train.h"
#include "mcts/ai.h"
#include "record.h"

#include <thread>

class GameContext {
 public:
  GameContext(const ContextOptions& context_options, const GameOptions& options)
      : _context_options(context_options), _go_feature(options) {
    _context.reset(new elf::Context);

    auto net_options = get_net_options(context_options, options);
    auto curr_timestamp = time(NULL);

    bool perform_training = false;

    int num_games = context_options.num_games;

    if (options.mode == "selfplay") {
      _writer.reset(new elf::shared::Writer(net_options));
      std::cout << _writer->info() << std::endl;
      _eval_ctrl.reset(new EvalCtrl(
          _context->getClient(), _writer.get(), options, num_games));

      std::cout << "Send ctrl " << curr_timestamp << std::endl;
      _writer->Ctrl(std::to_string(curr_timestamp));
    } else if (options.mode == "online") {
      _eval_ctrl.reset(new EvalCtrl(
          _context->getClient(), _writer.get(), options, num_games));
    } else if (options.mode == "train") {
      init_reader(num_games, options, context_options.mcts_options);
      _online_loader.reset(new DataOnlineLoader(*_reader, net_options));

      auto start_func = [&]() { _train_ctrl->RegRecordSender(); };

      auto replier = [&](elf::shared::Reader* reader,
                         const std::string& identity,
                         std::string* msg) -> bool {
        (void)reader;
        // cout << "Replier: before sending msg to " << identity << endl;
        _train_ctrl->onReply(identity, msg);
        // cout << "Replier: about to send to " << identity << ", msg = " <<
        // *msg << endl; cout << _reader->info() << endl;
        return true;
      };

      _online_loader->start(start_func, replier);
      perform_training = true;

    } else if (options.mode == "offline_train") {
      init_reader(num_games, options, context_options.mcts_options);
      _offline_loader.reset(
          new DataOfflineLoaderJSON(*_reader, options.list_files));
      _offline_loader->start();
      std::cout << _reader->info() << std::endl;
      _train_ctrl->RegRecordSender();
      perform_training = true;

    } else {
      std::cout << "Option.mode not recognized!" << options.mode << std::endl;
      throw std::range_error("Option.mode not recognized! " + options.mode);
    }

    const int batchsize = context_options.batchsize;

    // Register all functions.
    _go_feature.registerExtractor(batchsize, _context->getExtractor());

    if (perform_training) {
      for (int i = 0; i < num_games; ++i) {
        _games.emplace_back(new GoGameTrain(
            i,
            _context->getClient(),
            context_options,
            options,
            _train_ctrl.get(),
            _reader.get()));
      }
    } else {
      for (int i = 0; i < num_games; ++i) {
        _games.emplace_back(new GoGameSelfPlay(
            i,
            _context->getClient(),
            context_options,
            options,
            _eval_ctrl.get()));
      }
    }

    _context->setStartCallback(num_games, [this](int i, elf::GameClient*) {
      if (_eval_ctrl != nullptr)
        _eval_ctrl->RegGame(i);
      _games[i]->mainLoop();
    });

    _context->setCBAfterGameStart(
        [this, options]() { load_offline_selfplay_data(options); });
  }

  std::map<std::string, int> getParams() const {
    return _go_feature.getParams();
  }

  const GoGameBase* getGame(int game_idx) const {
    if (_check_game_idx(game_idx)) {
      std::cout << "Invalid game_idx [" + std::to_string(game_idx) + "]"
                << std::endl;
      return nullptr;
    }

    return _games[game_idx].get();
  }

  GameStats* getGameStats() {
    return &_eval_ctrl->getGameStats();
  }

  void waitForSufficientSelfplay(int64_t selfplay_ver) {
    _train_ctrl->waitForSufficientSelfplay(selfplay_ver);
  }

  // Used in training side.
  void notifyNewVersion(int64_t selfplay_ver, int64_t new_version) {
    _train_ctrl->addNewModelForEvaluation(selfplay_ver, new_version);
  }

  void setInitialVersion(int64_t init_version) {
    _train_ctrl->setInitialVersion(init_version);
  }

  void setEvalMode(int64_t new_ver, int64_t old_ver) {
    _train_ctrl->setEvalMode(new_ver, old_ver);
  }

  // Used in client side.
  void setRequest(
      int64_t black_ver,
      int64_t white_ver,
      float thres,
      int num_thread = -1) {
    MsgRequest request;
    request.vers.black_ver = black_ver;
    request.vers.white_ver = white_ver;
    request.vers.mcts_opt = _context_options.mcts_options;
    request.client_ctrl.black_resign_thres = thres;
    request.client_ctrl.white_resign_thres = thres;
    request.client_ctrl.num_game_thread_used = num_thread;
    _eval_ctrl->sendRequest(request);
  }

  elf::Context* ctx() {
    return _context.get();
  }

  ~GameContext() {
    // cout << "Ending train ctrl" << endl;
    _train_ctrl.reset(nullptr);

    // cout << "Ending eval ctrl" << endl;
    _eval_ctrl.reset(nullptr);

    // cout << "Ending offline loader" << endl;
    _offline_loader.reset(nullptr);

    // cout << "Ending online loader" << endl;
    _online_loader.reset(nullptr);

    // cout << "Ending reader" << endl;
    _reader.reset(nullptr);

    // cout << "Ending writer" << endl;
    _writer.reset(nullptr);

    // cout << "Ending games" << endl;
    _games.clear();

    // cout << "Ending context" << endl;
    _context.reset(nullptr);

    // cout << "Finish all ..." << endl;
  }

 private:
  std::unique_ptr<elf::Context> _context;
  std::vector<std::unique_ptr<GoGameBase>> _games;

  ContextOptions _context_options;

  std::unique_ptr<TrainCtrl> _train_ctrl;
  std::unique_ptr<EvalCtrl> _eval_ctrl;

  std::unique_ptr<elf::shared::Writer> _writer;
  std::unique_ptr<elf::shared::ReaderQueuesT<Record>> _reader;

  std::unique_ptr<DataOfflineLoaderJSON> _offline_loader;
  std::unique_ptr<DataOnlineLoader> _online_loader;

  GoFeature _go_feature;

  elf::shared::Options get_net_options(
      const ContextOptions& context_options,
      const GameOptions& options) {
    elf::shared::Options net_options;
    net_options.addr =
        options.server_addr == "" ? "localhost" : options.server_addr;
    net_options.port = options.port;
    net_options.use_ipv6 = true;
    net_options.verbose = options.verbose;
    net_options.identity = context_options.job_id;

    return net_options;
  }

  void load_offline_selfplay_data(const GameOptions& options) {
    if (options.list_files.empty())
      return;

    std::atomic<int> count(0);
    const size_t num_thread = 16;

    auto thread_main = [&options, this, &count, num_thread](size_t idx) {
      for (size_t k = 0; k * num_thread + idx < options.list_files.size();
           ++k) {
        const std::string& f = options.list_files[k * num_thread + idx];
        std::cout << "Load offline data: Reading: " << f << std::endl;

        std::vector<Record> records;
        if (!Record::loadBatchFromJsonFile(f, &records)) {
          std::cout << "Offline data loading: Error reading " << f << std::endl;
          return;
        }

        for (auto& r : records) {
          r.offline = true;
        }

        std::vector<FeedResult> res = _train_ctrl->onSelfplayGames(records);

        std::mt19937 rng(time(NULL));

        // If the record does not fit in _train_ctrl,
        // we should just send it directly to the replay buffer.
        for (size_t i = 0; i < records.size(); ++i) {
          if (res[i] == FeedResult::FEEDED ||
              res[i] == FeedResult::VERSION_MISMATCH) {
            bool black_win = records[i].result.reward > 0;
            _reader->InsertWithParity(std::move(records[i]), &rng, black_win);
            count++;
          }
        }
      }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_thread; ++i) {
      threads.emplace_back(std::bind(thread_main, i));
    }

    for (auto& t : threads) {
      t.join();
    }

    std::cout << "All offline data are loaded. #record read: " << count
              << " from " << options.list_files.size() << " files."
              << std::endl;
    std::cout << _reader->info() << std::endl;
  }

  void init_reader(
      int num_games,
      const GameOptions& options,
      const elf::ai::tree_search::TSOptions& mcts_opt) {
    elf::shared::RQCtrl ctrl;
    ctrl.num_reader = options.num_reader;
    ctrl.ctrl.queue_min_size = options.q_min_size;
    ctrl.ctrl.queue_max_size = options.q_max_size;

    auto converter =
        [this](const std::string& s, std::vector<Record>* rs) -> bool {
      if (rs == nullptr)
        return false;
      try {
        _train_ctrl->onReceive(s);
        rs->clear();
        return true;
      } catch (...) {
        std::cout << "Data malformed! ..." << std::endl;
        std::cout << s << std::endl;
        return false;
      }
    };

    _reader.reset(new elf::shared::ReaderQueuesT<Record>(ctrl));
    _train_ctrl.reset(new TrainCtrl(
        num_games, _context->getClient(), _reader.get(), options, mcts_opt));
    _reader->setConverter(converter);
    std::cout << _reader->info() << std::endl;
  }

  bool _check_game_idx(int game_idx) const {
    return game_idx < 0 || game_idx >= (int)_games.size();
  }
};
