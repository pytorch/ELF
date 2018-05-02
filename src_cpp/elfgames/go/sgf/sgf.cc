/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "sgf.h"

#include <fstream>
#include <functional>
#include <sstream>

static std::string trim(const std::string& str) {
  int l = 0;
  while (l < (int)str.size() && (str[l] == ' ' || str[l] == '\n'))
    l++;
  int r = str.size() - 1;
  while (r >= 0 && (str[r] == ' ' || str[r] == '\n'))
    r--;

  return str.substr(l, r + 1);
}

// [start, end)
typedef std::pair<int, int> seg;

bool Sgf::load(const std::string& filename, const std::string& game_string) {
  const char* str = game_string.c_str();
  int len = game_string.size();

  _header.reset();
  int next_offset = 0;
  if (load_header(str, seg(0, len), &next_offset)) {
    _root.reset(load(str, seg(next_offset, len), &next_offset));
    // cout << "Next offset = " << next_offset << " len = " << len << endl;
    // cout << printHeader();
    // cout << printMainVariation();

    auto iter = begin();
    if (iter.done())
      return false;

    // Compute the length of the move.
    _num_moves = 0;
    Coord last_move = M_INVALID;

    while (!iter.done()) {
      _num_moves++;
      ++iter;
      if (iter.getCurrMove().move == M_PASS && last_move == M_PASS)
        break;
      last_move = iter.getCurrMove().move;
    }
    return true;
  } else {
    std::cout << "Failed to read the header of " << filename << std::endl;
  }
  return false;
}

bool Sgf::load(const std::string& filename) {
  // std::cout << "Loading SGF: " << filename << std::endl;
  // Load the game.
  std::ifstream iFile(filename);
  std::stringstream ss;
  ss << iFile.rdbuf();
  std::string s = ss.str();
  return load(filename, s);
}

#define STATE_KEY 0
#define STATE_VALUE 1

// return next offset.
static int get_key_values(
    const char* s,
    const seg& range,
    std::function<void(const char*, const seg&, const seg&)> cb) {
  int i;
  seg key, value;
  int state = STATE_KEY;
  int start_idx = range.first;
  bool done = false;
  bool backslash = false;

  // cout << "Begin calling get_key_values with [" << range.first << ", " <<
  // range.second << ")" << endl;
  for (i = range.first; i < range.second && !done; ++i) {
    if (s[i] == '\\') {
      backslash = !backslash;
      continue;
    }
    if (backslash) {
      backslash = false;
      continue;
    }

    char c = s[i];
    // std::cout << "Next c: " << c << endl;
    switch (state) {
      case STATE_KEY:
        if (c == '[') {
          // Finish a key and start a value.
          key = seg(start_idx, i);
          start_idx = i + 1;
          state = STATE_VALUE;
        } else if (c == ';' || c == ')') {
          --i;
          done = true;
        }
        break;
      case STATE_VALUE:
        if (c == ']') {
          // Finish a value and start a key
          value = seg(start_idx, i);
          // Now we have complete key/value pairs.
          cb(s, key, value);
          start_idx = i + 1;
          state = STATE_KEY;
        }
        break;
    }
  }

  // cout << "End calling get_key_values with [" << range.first << ", " <<
  // range.second << "). next_offset = " << i << endl;
  return i;
}

static std::string make_str(const char* s, const seg& g) {
  return std::string(s + g.first, g.second - g.first);
}

static void save_sgf_header(
    SgfHeader* header,
    const char* s,
    const seg& key,
    const seg& value) {
  std::string v = trim(make_str(s, value));
  std::string k = trim(make_str(s, key));

  // std::cout << "SGF_Header: \"" << k << "\" = \"" <<  v << "\"" << std::endl;
  if (k == "RE") {
    if (!v.empty()) {
      header->winner = (v[0] == 'B' || v[0] == 'b') ? S_BLACK : S_WHITE;
      if (v.size() >= 3) {
        try {
          header->win_margin = stof(v.substr(2));
        } catch (...) {
          header->win_reason = v.substr(2);
        }
      }
    }
  } else if (k == "SZ") {
    header->size = stoi(v);
  } else if (k == "PW") {
    header->white_name = v;
  } else if (k == "PB") {
    header->black_name = v;
  } else if (k == "WR") {
    header->white_rank = v;
  } else if (k == "BR") {
    header->black_rank = v;
  } else if (k == "C") {
    header->comment = v;
  } else if (k == "KM") {
    header->komi = stof(v);
  } else if (k == "HA") {
    header->handi = stoi(v);
  }
}

bool Sgf::load_header(const char* s, const seg& range, int* next_offset) {
  // Load the header.
  int i = range.first;
  // std::cout << "[" << range.first << ", " << range.second << ")" <<
  // std::endl;
  while (s[i] != ';' && i < range.second) {
    // std::cout << "Char[" << i << "]: " << s[i] << std::endl;
    i++;
  }
  if (s[i] != ';')
    return false;
  i++;
  // Now we have header.
  *next_offset = get_key_values(
      s,
      seg(i, range.second),
      [&](const char* _s, const seg& key, const seg& value) {
        save_sgf_header(&_header, _s, key, value);
      });
  return true;
}

static void save_sgf_entry(
    SgfEntry* entry,
    const char* s,
    const seg& key,
    const seg& value) {
  std::string v = trim(make_str(s, value));
  std::string k = trim(make_str(s, key));

  if (k.size() == 1) {
    if (k[0] == 'B') {
      entry->player = S_BLACK;
      entry->move = str2coord(v);
    } else if (k[0] == 'W') {
      entry->player = S_WHITE;
      entry->move = str2coord(v);
    } else if (k[0] == 'C') {
      entry->comment = v;
    }
  } else {
    // Default key/value pairs.
    entry->kv.insert(make_pair(k, v));
  }
}

SgfEntry* Sgf::load(const char* s, const seg& range, int* next_offset) {
  // Build the tree recursively.
  *next_offset = 0;
  int i = range.first;
  const int e = range.second;

  while (i < e && s[i] != ';')
    ++i;
  if (i >= e)
    return nullptr;
  ++i;

  SgfEntry* entry = new SgfEntry;
  if (s[i] == '(') {
    ++i;
    // Recursion.
    entry->child.reset(load(s, seg(i, e), next_offset));
    if (s[*next_offset] != ')') {
      // Corrupted file.
      return nullptr;
    }
    (*next_offset)++;
    entry->sibling.reset(load(s, seg(*next_offset, e), next_offset));
  } else {
    *next_offset = get_key_values(
        s, seg(i, e), [&](const char* _s, const seg& key, const seg& value) {
          save_sgf_entry(entry, _s, key, value);
        });
    entry->sibling.reset(load(s, seg(*next_offset, e), next_offset));
  }
  return entry;
}

std::string Sgf::printHeader() const {
  std::stringstream ss;

  ss << "Win: " << STR_STONE(_header.winner) << " by " << _header.win_margin;
  if (!_header.win_reason.empty())
    ss << " Reason: " << _header.win_reason;
  ss << std::endl;
  ss << "Komi: " << _header.komi << std::endl;
  ss << "Handi: " << _header.handi << std::endl;
  ss << "Size: " << _header.size << std::endl;
  ss << "White: " << _header.white_name << "[" << _header.white_rank << "]"
     << std::endl;
  ss << "Black: " << _header.black_name << "[" << _header.black_rank << "]"
     << std::endl;
  ss << "Comment: " << _header.comment << std::endl;
  return ss.str();
}

std::string Sgf::printMainVariation() {
  std::stringstream ss;
  auto iter = begin();
  while (!iter.done()) {
    auto curr = iter.getCurrMove();
    ss << "[" << iter.getCurrIdx() << "]: " << STR_STONE(curr.player) << " "
       << coord2str(curr.move);
    std::string s = iter.getCurrComment();
    if (!s.empty())
      ss << " Comment: " << s;
    ss << std::endl;
    ++iter;
  }
  return ss.str();
}
