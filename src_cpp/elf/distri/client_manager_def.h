#pragma once

#include <nlohmann/json.hpp>
#include "elf/utils/json_utils.h"

namespace elf {

namespace cs {

using json = nlohmann::json;

using ClientType = int;
constexpr ClientType CLIENT_INVALID = -1;

struct ClientCtrl {
  ClientType client_type = 0;
  int seq = -1;

  // -1 means to use all the threads.
  int num_game_thread_used = -1;

  void setJsonFields(json& j) const {
    JSON_SAVE(j, client_type);
    JSON_SAVE(j, seq);
    JSON_SAVE(j, num_game_thread_used);
  }
  static ClientCtrl createFromJson(const json& j) {
    ClientCtrl ctrl;
    JSON_LOAD(ctrl, j, client_type);
    JSON_LOAD(ctrl, j, seq);
    JSON_LOAD(ctrl, j, num_game_thread_used);
    return ctrl;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[client=" << client_type << "]"
       << "[#th=" << num_game_thread_used << "]"
       << "[seq=" << seq << "]";
    return ss.str();
  }

  friend bool operator==(const ClientCtrl& c1, const ClientCtrl& c2) {
    return c1.client_type == c2.client_type &&
        c1.seq == c2.seq &&
        c1.num_game_thread_used == c2.num_game_thread_used;
  }
  friend bool operator!=(const ClientCtrl& c1, const ClientCtrl& c2) {
    return !(c1 == c2);
  }
};

}  // namespace cs 

}  // namespace elf
