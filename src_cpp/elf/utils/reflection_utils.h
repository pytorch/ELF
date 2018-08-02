#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace elf_utils {
namespace reflection {

class Printer {
 public:
  Printer(std::string prefix = "") : prefix_(prefix) {}

  template <typename C>
  std::string info(const C& c) {
    infos_.clear();
    c.apply(*this);

    using Key = std::pair<std::string, std::string>;
    // Reorder.
    std::sort(infos_.begin(), infos_.end(), [](const Key& s1, const Key& s2) {
      return s1.first < s2.first;
    });

    std::stringstream ss;
    for (const Key& k : infos_) {
      ss << k.second;
    }
    return ss.str();
  }

  template <typename T>
  void visit(std::string name, const std::vector<T>& entry, std::string help) {
    std::stringstream ss;
    ss << prefix_ << name << " [" << help << "]: ";
    for (const T& k : entry)
      ss << k << ", ";
    ss << std::endl;
    infos_.push_back(std::make_pair(name, ss.str()));
  }

  void visit(std::string name, bool entry, std::string help) {
    if (entry) {
      std::string info = prefix_ + name + " [" + help + "]: True\n";
      infos_.push_back(std::make_pair(name, info));
    }
  }

  template <typename T>
  void visit(std::string name, const T& entry, std::string help) {
    if constexpr (elf::reflection::has_reflection<T>::value) {
      Printer printer(prefix_ + name + ".");
      infos_.push_back(std::make_pair(name, printer.template info<T>(entry)));
    } else {
      std::stringstream ss;
      ss << prefix_ << name << " [" << help << "]: " << entry << std::endl;
      infos_.push_back(std::make_pair(name, ss.str()));
    }
  }

 private:
  std::string prefix_;
  std::vector<std::pair<std::string, std::string>> infos_;
};

} // namespace reflection
} // namespace elf_utils
