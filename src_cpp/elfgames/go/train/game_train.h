/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../common/go_game_specific.h"
#include "../common/go_state_ext.h"
#include "../common/record.h"
#include "elf/distri/game_interface.h"

using elf::cs::ServerGame;
using elf::cs::ReplayBuffer;

class GoGameTrain : public ServerGame {
  public:
    GoGameTrain(
        int game_idx,
        const GameOptionsTrain& options) {
      for (size_t i = 0; i < kNumState; ++i) {
        _state_ext.emplace_back(new GoStateExtOffline(game_idx, options));
      }
    }

    void step(elf::game::Base *base, ReplayBuffer *replay_buffer) override {
      std::vector<elf::FuncsWithState> funcsToSend;
      auto *client = base->client();
      auto binder = client->getBinder();

      for (size_t i = 0; i < kNumState; ++i) {
        while (true) {
          // std::cout << "[" << _game_idx << "][" << i << "] Before get sampler "
          // << std::endl;
          int q_idx;
          auto sampler = replay_buffer->getSamplerWithParity(&base->rng(), &q_idx);
          const Record* r = sampler.sample();
          // std::cout << "[" << _game_idx << "] Get Sampler, q_idx: " << q_idx <<
          // "/" << reader_->nqueue() << std::endl;
          if (r == nullptr) {
            // std::cout << "[" << _game_idx << "][" << i << "] No data, wait.." <<
            // std::endl;
            continue;
          }
          // std::cout << "[" << _game_idx << "][" << i << "] Has data.." <<
          // std::endl;
          _state_ext[i]->fromRecord(*r);

          // Random pick one ply.
          if (_state_ext[i]->switchRandomMove(&base->rng()))
            break;
        }

        // std::cout << "[" << _game_idx << "] Generating D4Code.." << endl;
        _state_ext[i]->generateD4Code(&base->rng());

        // elf::FuncsWithState funcs =
        // client_->BindStateToFunctions({"train"}, &_state_ext);
        funcsToSend.push_back(binder.BindStateToFunctions(
              {"train"}, _state_ext[i].get()));
      }

      // client_->sendWait({"train"}, &funcs);

      std::vector<elf::FuncsWithState*> funcPtrsToSend(funcsToSend.size());
      for (size_t i = 0; i < funcsToSend.size(); ++i) {
        funcPtrsToSend[i] = &funcsToSend[i];
      }

      // VERY DANGEROUS - sending pointers of local objects to a function
      // std::cout << "[" << _game_idx << "] Sending packages to python.." <<
      // std::endl;
      client->sendBatchWait({"train"}, funcPtrsToSend);
      // std::cout << "[" << _game_idx << "] Return from python ..." << std::endl;
    }

  private:
    static constexpr size_t kNumState = 64;
    std::vector<std::unique_ptr<GoStateExtOffline>> _state_ext;
};
