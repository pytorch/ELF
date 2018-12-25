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

  void setJsonFields(json& j) const {
    JSON_SAVE(j, client_type);
    JSON_SAVE(j, seq);
  }
  static ClientCtrl createFromJson(const json& j) {
    ClientCtrl ctrl;
    JSON_LOAD(ctrl, j, client_type);
    JSON_LOAD(ctrl, j, seq);
    return ctrl;
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[client=" << client_type << "]"
       << "[seq=" << seq << "]";
    return ss.str();
  }

  friend bool operator==(const ClientCtrl& c1, const ClientCtrl& c2) {
    return c1.client_type == c2.client_type && c1.seq == c2.seq;
  }
  friend bool operator!=(const ClientCtrl& c1, const ClientCtrl& c2) {
    return !(c1 == c2);
  }
};

}  // namespace cs 

}  // namespace elf
