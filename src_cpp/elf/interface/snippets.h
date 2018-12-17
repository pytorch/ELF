#pragma once

#include <unordered_map>
#include <string>
#include <vector>

namespace elf {

using SpecItem = std::unordered_map<std::string, std::vector<std::string>>;
using Spec = std::unordered_map<std::string, SpecItem>;

namespace snippet {

struct DiscreteReply {
  int64_t a;
  float V;
  float r;
  int terminal;
  std::vector<float> pi;

  DiscreteReply(int num_action = 0) : pi(num_action) {
  }

  void setPi(const float *ppi) { assert(pi.size() > 0); std::copy(ppi, ppi + pi.size(), pi.begin()); }
  void setValue(const float *pV) { V = *pV; }
  void setAction(const int64_t *aa) { a = *aa; }

  size_t getValue(float *pV) const { *pV = V; return 1; }
  size_t getAction(int64_t *aa) const { *aa = a; return 1; }
  size_t getPi(float *ppi) const { assert(pi.size() > 0); std::copy(pi.begin(), pi.end(), ppi); return pi.size(); }
  size_t getReward(float *rr) const { *rr = r; return 1; }
  size_t getTerminal(int *tterminal) const { *tterminal = r; return 1; }
};

} // namespace snippet

} // namespace elf
