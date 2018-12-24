/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "model_pair.h"

#include "../base/board.h"
#include "../base/common.h"

struct MsgVersion {
  int64_t model_ver;
  MsgVersion(int ver = -1) : model_ver(ver) {}
};

struct Request {
  ModelPair vers;
  float resign_thres = 0.0;
  float never_resign_prob = 0.0;

  bool player_swap = false;
  bool async = false;

  void setJsonFields(json& j) const {
    JSON_SAVE_OBJ(j, vers);
    JSON_SAVE(j, resign_thres);
    JSON_SAVE(j, never_resign_prob);
    JSON_SAVE(j, player_swap);
    JSON_SAVE(j, async);
  }

  static Request createFromJson(const json& j) {
    Request request;
    JSON_LOAD_OBJ(request, j, vers);
    // What does the following do?
    // JSON_LOAD_OBJ(request, j, client_ctrl, request.vers.is_selfplay());
    JSON_LOAD(request, j, resign_thres);
    JSON_LOAD(request, j, never_resign_prob);
    JSON_LOAD(request, j, player_swap);
    JSON_LOAD(request, j, async);
    return request;
  }

  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[async=" << async << "]"
       << "[res_th=" << resign_thres << "][swap=" << player_swap
       << "][never_res_pr=" << never_resign_prob << "]";
    return ss.str();
  }

  friend bool operator==(const Request& r1, const Request& r2) {
    return r1.resign_thres == r2.resign_thres &&
      r1.never_resign_prob == r2.never_resign_prob &&
      r1.player_swap == r2.player_swap &&
      r1.async == r2.async;
  }

  friend bool operator!=(const Request& r1, const Request& r2) {
    return !(r1 == r2);
  }
};

