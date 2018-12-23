#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <random>
#include <vector>

#include "client_manager.h"
#include "record.h"

namespace elf {

namespace cs {

using json = nlohmann::json;

class GameInterface {
 public:
   virtual json to_json() const = 0;
   virtual void step() = 0;
   virtual std::unordered_map<std::string, int> getParams() const = 0;
};

using ReplayBuffer = elf::shared::ReaderQueuesT<Record>;

class ServerInterface {
 public:
  virtual void OnStart() = 0;
  virtual elf::shared::InsertInfo OnReceive(const Records &rs, const ClientInfo& info, ReplayBuffer *replay_buffer) = 0;
  virtual void fillInRequest(const ClientInfo &info, MsgRequest *request) = 0;
};

class Factory {
 public:
   std::function<std::unique_ptr<GameInterface> (const json &, std::mt19937 *)> gameFromJson;
   std::function<std::unique_ptr<ServerInterface> ()> getServerInterface;
};

}  // namespace cs

}  // namespace elf
