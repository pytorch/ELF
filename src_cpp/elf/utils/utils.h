/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// TODO - replace this with something appropriate from C++ (ssengupta@fb)
#include <time.h>

namespace elf_utils {

inline std::string print_bool(bool b) {
  return b ? "True" : "False";
}

inline std::string now() {
  time_t t = time(nullptr);
  std::string time_str = asctime(localtime(&t));
  return time_str.substr(0, time_str.size() - 1);
}

inline std::string time_signature() {
  time_t t = std::time(nullptr);
  char mbstr[100];
  std::strftime(mbstr, sizeof(mbstr), "%y%m%d-%H%M%S", std::localtime(&t));
  return std::string(mbstr);
}

inline uint64_t sec_since_epoch_from_now() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(
             now.time_since_epoch())
      .count();
}

inline uint64_t get_seed(int game_idx) {
  // [TODO] Definitely not the right way, but working.
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  auto value = now_ms.time_since_epoch();
  long duration = value.count();
  return (time(NULL) * 1000 + duration + game_idx * 2341479) % 100000000;
}

// Input a sorted list.
inline std::string get_gap_list(const std::vector<int>& l) {
  if (l.empty())
    return "";
  int last = l[0];
  size_t last_i = 0;
  std::string output = std::to_string(last);

  for (size_t i = 1; i < l.size(); ++i) {
    if (l[i] > last + static_cast<int>(i - last_i)) {
      if (l[i - 1] > last)
        output += "-" + std::to_string(l[i - 1]);
      last = l[i];
      last_i = i;
      output += ", " + std::to_string(last);
    } else if (i == l.size() - 1) {
      output += "-" + std::to_string(l[i]);
    }
  }
  return output;
}

inline std::string trim(std::string& str) {
  str.erase(0, str.find_first_not_of(' ')); // prefixing spaces
  str.erase(str.find_last_not_of(' ') + 1); // surfixing spaces
  return str;
}

inline std::vector<std::string> split(const std::string& s, char delim) {
  std::stringstream ss(s);
  std::string item;
  std::vector<std::string> elems;
  while (getline(ss, item, delim)) {
    elems.push_back(move(item));
  }
  return elems;
}

template <typename Map>
const typename Map::mapped_type& map_get(
    const Map& m,
    const typename Map::key_type& k,
    const typename Map::mapped_type& def) {
  auto it = m.find(k);
  if (it == m.end())
    return def;
  else
    return it->second;
}

template <typename Map>
const typename Map::mapped_type& map_inc(
    Map& m,
    const typename Map::key_type& k,
    const typename Map::mapped_type& default_value) {
  auto it = m.find(k);
  if (it == m.end()) {
    auto res = m.insert(std::make_pair(k, default_value));
    return res.first->second;
  } else {
    it->second++;
    return it->second;
  }
}

/*
template <typename Map>
typename Map::mapped_type map_get(const Map &m, const typename Map::key_type& k,
typename Map::mapped_type def) {
    auto it = m.find(k);
    if (it == m.end()) return def;
    else return it->second;
}
*/

template <typename Map>
std::pair<typename Map::const_iterator, bool> map_get(
    const Map& m,
    const typename Map::key_type& k) {
  auto it = m.find(k);
  if (it == m.end()) {
    return std::make_pair(m.end(), false);
  } else {
    return std::make_pair(it, true);
  }
}

template <typename Map>
std::pair<typename Map::iterator, bool> map_get(
    Map& m,
    const typename Map::key_type& k) {
  auto it = m.find(k);
  if (it == m.end()) {
    return std::make_pair(m.end(), false);
  } else {
    return std::make_pair(it, true);
  }
}

template <typename A>
size_t sample_multinomial(
    const std::vector<std::pair<A, float>>& v,
    std::mt19937* gen) {
  std::vector<float> accu(v.size() + 1);

  float Z = 0.0;
  for (const auto& vv : v) {
    Z += vv.second;
  }

  std::uniform_real_distribution<> dis(0, Z);
  float rd = dis(*gen);

  accu[0] = 0;
  for (size_t i = 1; i < accu.size(); i++) {
    accu[i] = v[i - 1].second + accu[i - 1];
    if (rd < accu[i]) {
      return i - 1;
    }
  }

  return v.size() - 1;
}

class MyClock {
 private:
  std::chrono::time_point<std::chrono::system_clock> _time_start;
  std::map<std::string, std::pair<std::chrono::duration<double>, int>>
      _durations;

 public:
  MyClock() {}
  void restart() {
    for (auto it = _durations.begin(); it != _durations.end(); ++it) {
      it->second.first = std::chrono::duration<double>::zero();
      it->second.second = 0;
    }
    _time_start = std::chrono::system_clock::now();
  }

  void setStartPoint() {
    _time_start = std::chrono::system_clock::now();
  }

  std::string summary() const {
    std::stringstream ss;
    double total_time = 0;
    for (auto it = _durations.begin(); it != _durations.end(); ++it) {
      if (it->second.second > 0) {
        double v = it->second.first.count() * 1000 / it->second.second;
        ss << it->first << ": " << v << "ms. ";
        total_time += v;
      }
    }
    ss << "Total: " << total_time << "ms.";
    return ss.str();
  }

  inline bool record(const std::string& item) {
    // cout << "Record: " << item << endl;
    auto it = _durations.find(item);
    if (it == _durations.end()) {
      it = _durations
               .insert(std::make_pair(
                   item, std::make_pair(std::chrono::duration<double>(0), 0)))
               .first;
    }

    auto time_tmp = std::chrono::system_clock::now();
    it->second.first += time_tmp - _time_start;
    it->second.second++;
    _time_start = time_tmp;
    return true;
  }
};

} // namespace elf_utils
