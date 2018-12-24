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

class ClientInterface {
 public:
   virtual bool onReceive(const json &state, MsgReply *reply) = 0;
   virtual ThreadState getThreadState() const = 0;
   virtual void onEnd(elf::game::Base*) = 0;
   virtual StepStatus step(elf::game::Base*, json *j) = 0;
   virtual std::unordered_map<std::string, int> getParams() const = 0;
};

using ReplayBuffer = elf::shared::ReaderQueuesT<Record>;

class ServerInterface {
 public:
  virtual void onStart() = 0;
  virtual elf::shared::InsertInfo onReceive(const Records &rs, const ClientInfo& info, ReplayBuffer *replay_buffer) = 0;
  virtual void fillInRequest(const ClientInfo &info, json *state) = 0;
};

class GameInterface {
 public:
   virtual void fromJson(const json &) = 0;
   // Return true if we want this game to be sent.
   virtual bool step(elf::game::Base *) = 0;
};

struct ServerFactory {
  std::function<std::unique_ptr<GameInterface> (int)> createGameInterface = nullptr;
  std::function<std::unique_ptr<ServerInterface> ()> createServerInterface = nullptr;
  std::function<std::unordered_map<std::string, int> ()> getParams = nullptr;
};

struct ClientFactory {
  std::function<std::unique_ptr<ClientInterface> (int)> createClientInterface = nullptr;
  std::function<void (const Addr &, MsgRequest*)> onFirstSend = nullptr;
  std::function<std::vector<bool> (const std::vector<MsgRequest>&, std::vector<MsgReply>*)> onReply = nullptr;
  std::function<std::unordered_map<std::string, int> ()> getParams = nullptr;
};

}  // namespace cs

}  // namespace elf
