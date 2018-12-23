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

  float resign_thres = 0.0;
  float never_resign_prob = 0.0;

  bool player_swap = false;
  bool async = false;

  void setJsonFields(json& j) const {
    JSON_SAVE(j, client_type);
    JSON_SAVE(j, seq);
    JSON_SAVE(j, num_game_thread_used);
    JSON_SAVE(j, resign_thres);
    JSON_SAVE(j, never_resign_prob);
    JSON_SAVE(j, player_swap);
    JSON_SAVE(j, async);
  }
  static ClientCtrl createFromJson(
      const json& j,
      bool player_swap_optional = false) {
    ClientCtrl ctrl;
    JSON_LOAD(ctrl, j, client_type);
    JSON_LOAD(ctrl, j, seq);
    JSON_LOAD(ctrl, j, num_game_thread_used);
    JSON_LOAD(ctrl, j, resign_thres);
    JSON_LOAD(ctrl, j, never_resign_prob);
    // For backward compatibility.
    if (player_swap_optional) {
      JSON_LOAD_OPTIONAL(ctrl, j, player_swap);
    } else {
      JSON_LOAD(ctrl, j, player_swap);
    }
    JSON_LOAD_OPTIONAL(ctrl, j, async);
    return ctrl;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[client=" << client_type << "][async=" << async << "]"
       << "[#th=" << num_game_thread_used << "]"
       << "[seq=" << seq << "]"
       << "[res_th=" << resign_thres << "][swap=" << player_swap
       << "][never_res_pr=" << never_resign_prob << "]";
    return ss.str();
  }

  friend bool operator==(const ClientCtrl& c1, const ClientCtrl& c2) {
    return c1.client_type == c2.client_type &&
        c1.seq == c2.seq &&
        c1.num_game_thread_used == c2.num_game_thread_used &&
        c1.resign_thres == c2.resign_thres &&
        c1.never_resign_prob == c2.never_resign_prob &&
        c1.player_swap == c2.player_swap && c1.async == c2.async;
  }
  friend bool operator!=(const ClientCtrl& c1, const ClientCtrl& c2) {
    return !(c1 == c2);
  }
};

}  // namespace cs 

}  // namespace elf
