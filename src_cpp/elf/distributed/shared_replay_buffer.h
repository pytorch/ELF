/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cassert>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

template <typename Key, typename Record>
class SharedReplayBuffer {
 public:
  using GenFunc = std::function<std::unique_ptr<Record>(const Key&)>;

  SharedReplayBuffer(GenFunc gen) : _gen(gen) {}
  void InitRecords(const std::vector<Key>& keys) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& key : keys)
      add_record_no_lock(key);
  }

  bool HasKey(const Key& key) const {
    return _buffer.find(key) != _buffer.end();
  }

  const Record& Get(const Key& key) {
    typename BufferType::const_iterator it = _buffer.find(key);
    if (it == _buffer.end()) {
      std::lock_guard<std::mutex> lock(_mutex);
      // Check again.
      it = _buffer.find(key);
      if (it == _buffer.end())
        it = add_record_no_lock(key);
    }
    return *it->second;
  }

 private:
  using BufferType = std::map<Key, std::unique_ptr<Record>>;

  BufferType _buffer;
  std::mutex _mutex;
  GenFunc _gen;

  typename BufferType::const_iterator add_record_no_lock(const Key& key) {
    assert(_gen != nullptr);
    return _buffer.emplace(make_pair(key, _gen(key))).first;
  }
};
