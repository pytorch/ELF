#pragma once

#include "../utils/reflection.h"
#include "OptionSpec.h"

namespace elf {
namespace options {

template <typename C>
class Visitor {
 public:
  Visitor(std::string prefix, OptionSpec& spec) 
    : prefix_(prefix), spec_(spec) {
    static_assert(elf::reflection::has_reflection<C>::value);
    C::applyStatic(*this);
  }

  template <typename T>
  void visit(std::string optionName, T defaultValue, std::string help) {
    if constexpr (elf::reflection::has_reflection<T>::value) {
      (void)defaultValue;
      Visitor<T> sub_visitor(prefix_ + optionName + ".", spec_);
    } else {
      spec_.template addOption<T>(prefix_ + optionName, help, defaultValue);
    }
  }

  void
  visit(std::string optionName, const char* defaultValue, std::string help) {
    spec_.template addOption<std::string>(
        prefix_ + optionName, help, std::string(defaultValue));
  }

 private:
  std::string prefix_;
  OptionSpec& spec_;
};

template <typename C>
class Loader {
 public:
  Loader(std::string prefix, OptionSpec& spec, const C& instance)
      : prefix_(prefix), spec_(spec) {
    static_assert(elf::reflection::has_reflection<C>::value);
    instance.apply(*this);
  }

  template <typename T>
  void visit(std::string optionName, const T& value, std::string help) const {
    if constexpr (elf::reflection::has_reflection<T>::value) {
      Loader<T> sub_loader(prefix_ + optionName + ".", spec_, value);
    } else {
      // std::cout << "Loading " << prefix_ + optionName << ", type: " <<
      // typeid(T).name() << std::endl;
      spec_.template addOption<T>(prefix_ + optionName, help, value);
    }
  }

 private:
  std::string prefix_;
  OptionSpec& spec_;
};

template <typename C>
class Saver {
 public:
  Saver(std::string prefix, const OptionSpec& spec, C& instance)
      : prefix_(prefix), spec_(spec) {
    static_assert(elf::reflection::has_reflection<C>::value);
    instance.applyMutable(*this);
  }

  template <typename T>
  void visit(std::string optionName, T& value, std::string) const {
    if constexpr (elf::reflection::has_reflection<T>::value) {
      Saver<T> sub_saver(prefix_ + optionName + ".", spec_, value);
    } else {
      // std::cout << "Loading " << prefix_ + optionName << ", type: " <<
      // typeid(T).name() << std::endl;
      value =
          spec_.template getOptionInfoTyped<T>(prefix_ + optionName).value();
    }
  }

 private:
  std::string prefix_;
  const OptionSpec& spec_;
};

} // namespace options
} // namespace elf
