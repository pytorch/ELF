#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <random>
#include <vector>

#include "elf/distributed/shared_reader.h"
#include "elf/interface/game_base.h"
#include "elf/base/ctrl.h"

#include "client_manager.h"
#include "record.h"

namespace elf {

namespace cs {

using json = nlohmann::json;
using MsgReply = int;

enum StepStatus {
  RUNNING = 0,
  NEW_RECORD,
  EPOSIDE_END,
};

class ClientGame {
 public:
   virtual bool onReceive(const MsgRequest &, MsgReply *reply) = 0;
   virtual ThreadState getThreadState() const = 0;
   virtual void onEnd(elf::game::Base*) = 0;
   virtual StepStatus step(elf::game::Base*, Record *) = 0;
};

struct ClientInterface {
 public:
  virtual void onFirstSend(const Addr &, MsgRequest*) = 0;
  virtual std::vector<bool> onReply(const std::vector<MsgRequest>&, std::vector<MsgReply>*) = 0;
  
  virtual ClientGame *createGame(int) = 0;
};

using ReplayBuffer = elf::shared::ReaderQueuesT<Record>;

class ServerGame {
 public:
   virtual void step(elf::game::Base *, ReplayBuffer *) = 0;
};

class ServerInterface {
 public:
  virtual void onStart() = 0;
  virtual elf::shared::InsertInfo onReceive(Records &&rs, const ClientInfo& info) = 0;
  virtual void fillInRequest(const ClientInfo &info, MsgRequest *) = 0;

  virtual ServerGame* createGame(int) = 0;
};

}  // namespace cs

}  // namespace elf
