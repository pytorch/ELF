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
#include "elf/utils/json_utils.h"

using json = nlohmann::json;

struct MsgRequest {
  int request = -1;

  std::string info() const {
    std::stringstream ss;
    ss << "[request=" << request << "]";
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, request);
  }

  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  static MsgRequest createFromJson(const json& j) {
    MsgRequest r;
    JSON_LOAD(r, j, request);
    return r;
  }

  friend bool operator==(const MsgRequest &m1, const MsgRequest &m2) {
    return m1.request == m2.request;
  }

  friend bool operator!=(const MsgRequest &m1, const MsgRequest &m2) {
    return ! (m1 == m2);
  }
};

struct MsgResult {
  int result = -1;
  std::string info() const {
    std::stringstream ss;
    ss << "[result=" << result << "]";
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, result);
  }

  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  static MsgResult createFromJson(const json& j) {
    MsgResult r;
    JSON_LOAD(r, j, result);
    return r;
  }
};

struct Record {
  MsgRequest request;
  MsgResult result;

  uint64_t timestamp = 0;
  uint64_t thread_id = 0;
  int seq = 0;

  std::string info() const {
    std::stringstream ss;
    ss << "[t=" << timestamp << "][id=" << thread_id << "][seq=" << seq
       << "]" << std::endl;
    // ss << request.info() << std::endl;
    // ss << result.info() << std::endl;
    return ss.str();
  }

  void setJsonFields(json& j) const {
    JSON_SAVE_OBJ(j, request);
    JSON_SAVE_OBJ(j, result);
    JSON_SAVE(j, timestamp);
    JSON_SAVE(j, thread_id);
    JSON_SAVE(j, seq);
  }

  static Record createFromJson(const json& j) {
    Record r;

    JSON_LOAD_OBJ(r, j, request);
    JSON_LOAD_OBJ(r, j, result);
    JSON_LOAD(r, j, timestamp);
    JSON_LOAD(r, j, thread_id);
    JSON_LOAD(r, j, seq);
    return r;
  }

  // Extra serialization.
  static std::vector<Record> createBatchFromJson(const std::string& json_str) {
    return createBatchFromJson(json::parse(json_str));
  }

  static std::vector<Record> createBatchFromJson(const json& j) {
    // cout << "from json_batch" << endl;
    std::vector<Record> records;
    for (size_t i = 0; i < j.size(); ++i) {
      try {
        records.push_back(createFromJson(j[i]));
      } catch (...) {
      }
    }
    return records;
  }

  static bool loadContent(const std::string& f, std::string* msg) {
    try {
      std::ifstream iFile(f.c_str());
      iFile.seekg(0, std::ios::end);
      size_t size = iFile.tellg();
      msg->resize(size, ' ');
      iFile.seekg(0);
      iFile.read(&(*msg)[0], size);
      return true;
    } catch (...) {
      return false;
    }
  }

  static bool loadBatchFromJsonFile(
      const std::string& f,
      std::vector<Record>* records) {
    assert(records != nullptr);

    try {
      std::string buffer;
      if (!loadContent(f, &buffer)) {
        return false;
      }
      *records = createBatchFromJson(buffer);
      return true;
    } catch (...) {
      return false;
    }
  }

  static std::string dumpBatchJsonString(
      std::vector<Record>::const_iterator b,
      std::vector<Record>::const_iterator e) {
    json j;
    for (auto it = b; it != e; ++it) {
      json j1;
      it->setJsonFields(j1);
      j.push_back(j1);
    }
    return j.dump();
  }
};

struct ThreadState {
  int thread_id = -1;
  // Which game we have played.
  int seq = 0;
  // Which move we have proceeded.
  int move_idx = 0;

  int64_t black = -1;
  int64_t white = -1;

  void setJsonFields(json& j) const {
    JSON_SAVE(j, thread_id);
    JSON_SAVE(j, seq);
    JSON_SAVE(j, move_idx);
    JSON_SAVE(j, black);
    JSON_SAVE(j, white);
  }

  static ThreadState createFromJson(const json& j) {
    ThreadState state;
    JSON_LOAD(state, j, thread_id);
    JSON_LOAD(state, j, seq);
    JSON_LOAD(state, j, move_idx);
    JSON_LOAD(state, j, black);
    JSON_LOAD(state, j, white);
    return state;
  }

  friend bool operator==(const ThreadState& t1, const ThreadState& t2) {
    return t1.thread_id == t2.thread_id && t1.seq == t2.seq &&
        t1.move_idx == t2.move_idx && t1.black == t2.black &&
        t1.white == t2.white;
  }

  friend bool operator!=(const ThreadState& t1, const ThreadState& t2) {
    return !(t1 == t2);
  }

  std::string info() const {
    std::stringstream ss;
    ss << "[th_id=" << thread_id << "][seq=" << seq << "][mv_idx=" << move_idx
       << "]"
       << "[black=" << black << "][white=" << white << "]";
    return ss.str();
  }
};

struct Records {
  std::string identity;
  std::unordered_map<int, ThreadState> states;
  std::vector<Record> records;

  Records() {}
  Records(const std::string& id) : identity(id) {}

  void clear() {
    states.clear();
    records.clear();
  }

  void addRecord(Record&& r) {
    records.emplace_back(r);
  }

  bool isRecordEmpty() const {
    return records.empty();
  }

  void updateState(const ThreadState& ts) {
    states[ts.thread_id] = ts;
  }

  void setJsonFields(json& j) const {
    JSON_SAVE(j, identity);
    for (const auto& t : states) {
      json jj;
      t.second.setJsonFields(jj);
      j["states"].push_back(jj);
    }

    for (const Record& r : records) {
      json j1;
      r.setJsonFields(j1);
      j["records"].push_back(j1);
    }
  }

  static Records createFromJson(const json& j) {
    Records rs;
    JSON_LOAD(rs, j, identity);
    if (j.find("states") != j.end()) {
      for (size_t i = 0; i < j["states"].size(); ++i) {
        ThreadState t = ThreadState::createFromJson(j["states"][i]);
        rs.states[t.thread_id] = t;
      }
    }

    if (j.find("records") != j.end()) {
      // cout << "from json_batch" << endl;
      for (size_t i = 0; i < j["records"].size(); ++i) {
        rs.records.push_back(Record::createFromJson(j["records"][i]));
      }
    }
    return rs;
  }

  std::string dumpJsonString() const {
    json j;
    setJsonFields(j);
    return j.dump();
  }

  static Records createFromJsonString(const std::string& s) {
    json j = json::parse(s);
    if (j.find("identity") == j.end()) {
      // This is a vector<Records>
      Records rs("");
      rs.records = Record::createBatchFromJson(s);
      return rs;
    } else {
      return createFromJson(j);
    }
  }
};
