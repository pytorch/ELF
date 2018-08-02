#pragma once

#include <nlohmann/json.hpp>
#include "../utils/base64.h"
#include "sharedmem.h"

using json = nlohmann::json;

namespace comm {
// WaitOptions
void to_json(json& j, const WaitOptions& opt) {
  j["batchsize"] = opt.batchsize;
  j["timeout_usec"] = opt.timeout_usec;
  j["min_batchsize"] = opt.min_batchsize;
}

void from_json(const json& j, WaitOptions& opt) {
  opt.batchsize = j["batchsize"];
  opt.timeout_usec = j["timeout_usec"];
  opt.min_batchsize = j["min_batchsize"];
}

// RecvOptions
void to_json(json& j, const RecvOptions& opt) {
  j["label"] = opt.label;
  j["wait_opt"] = opt.wait_opt;
}

void from_json(const json& j, RecvOptions& opt) {
  opt.label = j["label"];
  from_json(j["wait_opt"], opt.wait_opt);
}
} // namespace comm

namespace elf {

// SharedMemOptions
void to_json(json& j, const SharedMemOptions& smem) {
  j["idx"] = smem.getIdx();
  j["label_idx"] = smem.getLabelIdx();
  j["transfer_type"] = smem.getTransferType();
  j["recv_options"] = smem.getRecvOptions();
}

void from_json(const json& j, SharedMemOptions& smem) {
  // std::cout << "from_json SharedMemOptions: idx" << std::endl;
  // std::cout << j["idx"] << std::endl;
  smem.setIdx(j["idx"]);
  // std::cout << "from_json SharedMemOptions: label_idx" << std::endl;
  // std::cout << j["label_idx"] << std::endl;
  smem.setLabelIdx(j["label_idx"]);
  // std::cout << "from_json SharedMemOptions: transfer_type" << std::endl;
  // std::cout << j["transfer_type"] << std::endl;
  smem.setTransferType(j["transfer_type"]);
  from_json(j["recv_options"], smem.getRecvOptions());
}

// Stride
void to_json(json& j, const Size& sz) {
  j = sz.vec();
}

void from_json(const json& j, Size& sz) {
  // TODO could be more efficient.
  std::vector<int> l;
  for (const auto& jj : j) {
    l.push_back(jj);
  }
  sz = l;
}

// AnyP
void to_json(json& j, const AnyP& anyp) {
  j["name"] = anyp.field().getName();
  j["type_size"] = anyp.field().getSizeOfType();
  j["size_byte"] = anyp.getByteSize();

  j["p"] = base64_encode(anyp.getPtr(), j["size_byte"]);

  /*
  std::cout << "to_json: name: " << anyp.field().getName()
            << ", Stride: " << anyp.getStride().info()
            << "type_size: " << j["type_size"]
            << ", size_byte: " << j["size_byte"]
            << ", p = " << std::hex << reinterpret_cast<uint64_t>(anyp.getPtr())
  << std::dec << std::endl;

  if (anyp.field().getName() == "rv") {
    const int64_t *p = anyp.getAddress<int64_t>(0);
    std::cout << "to_json, size_byte: " << j["size_byte"]
              << ", Stride: " << anyp.getStride().info() << ", rv: ";
    for (int i = 0; i < anyp.field().getBatchSize(); i ++) {
      std::cout << p[i] << ",";
    }
    std::cout << std::endl;
  }
  */
}

void from_json(const json& j, AnyP& anyp) {
  assert(j["name"] == anyp.field().getName());
  assert(j["type_size"] == anyp.field().getSizeOfType());
  assert(j["size_byte"] == anyp.getByteSize());

  std::string content = base64_decode(j["p"]);
  assert(j["size_byte"] == content.size());
  ::memcpy(anyp.getPtr(), content.c_str(), content.size());

  /*
  std::cout << "from_json: name: " << anyp.field().getName()
            << ", Stride: " << anyp.getStride().info()
            << "type_size: " << j["type_size"]
            << ", size_byte: " << j["size_byte"]
            << ", p = " << std::hex << reinterpret_cast<uint64_t>(anyp.getPtr())
  << std::dec << std::endl;

  if (anyp.field().getName() == "rv") {
    int64_t *p = anyp.getAddress<int64_t>(0);
    std::cout << "from_json, size_byte: " << j["size_byte"]
              << ", Stride: " << anyp.getStride().info() << ", rv: ";
    for (int i = 0; i < anyp.field().getBatchSize(); i ++) {
      std::cout << p[i] << ",";
    }
    std::cout << std::endl;
  }
  */
}

// SharedMem
inline void SMemToJson(const SharedMemData& smem, const std::set<std::string> &keys, json& j) {
  j["opts"] = smem.getSharedMemOptionsC();
  for (const auto& p : smem.GetMem()) {
    if (keys.find(p.first) != keys.end())
      j["mem"][p.first] = p.second;
  }
  j["batchsize"] = smem.getEffectiveBatchSize();
  // std::cout << "to_json: effective batchsize: " << j["batchsize"] <<
  // std::endl;
}

inline void SMemToJsonExclude(const SharedMemData& smem, const std::set<std::string> &exclude_keys, json& j) {
  j["opts"] = smem.getSharedMemOptionsC();
  for (const auto& p : smem.GetMem()) {
    if (exclude_keys.find(p.first) == exclude_keys.end())
      j["mem"][p.first] = p.second;
  }
  j["batchsize"] = smem.getEffectiveBatchSize();
  // std::cout << "to_json: effective batchsize: " << j["batchsize"] <<
  // std::endl;
}

inline void SMemFromJson(const json& j, SharedMemData& smem) {
  // std::cout << "from_json: opt" << std::endl;
  // std::cout << j["opts"] << std::endl;
  // std::cout << "finish printing jopts" << std::endl;
  from_json(j["opts"], smem.getSharedMemOptions());

  auto jmem = j["mem"];

  for (auto& p : smem.GetMem()) {
    // std::cout << "from_json: mem: " << p.first << std::endl;
    if (jmem.find(p.first) != jmem.end())
      from_json(j["mem"][p.first], p.second);
  }

  // std::cout << "from_json: dealing with effective batchsize: " << std::endl;
  smem.setEffectiveBatchSize(j["batchsize"]);
  // std::cout << "from_json: effective batchsize: " << j["batchsize"] <<
  // std::endl;
}

} // namespace elf
