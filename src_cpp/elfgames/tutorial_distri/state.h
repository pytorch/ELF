#pragma once

#include <vector>
#include <string>
#include <sstream>

struct State {
  int content = -1;
  std::string info() const {
    std::stringstream ss;
    ss << "content: " << content;
    return ss.str();
  }
  friend bool operator==(const State &s1, const State &s2) {
    return s1.content == s2.content;
  }
};

struct Reply {
  int a = -1;
  float value = 0.0;
  std::vector<float> pi;

  std::string info() const {
    std::stringstream ss;
    ss << "value: " << value << ", a: " << a << ", pi: ";
    for (const float f : pi) {
      ss << f << ",";
    }
    return ss.str();
  }
};
