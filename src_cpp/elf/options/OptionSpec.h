/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * The OptionSpec class contains the specification for a collection of
 * config options.
 *
 * Internally, the options are represented as OptionBase objects but you don't
 * need to worry about that. All you need to know:
 *
 * bool addOption<type>(optionName, help)
 *   adds a required option with a help message
 *
 * bool addOption<type>(optionName, help, defaultValue)
 *   adds an optional option with a help message and default value
 *
 * nlohmann::json getPythonArgparseOptionsAsJSON()
 *   returns a JSON array with an object containing args and kwargs for each
 *     available option.
 *
 * void merge(otherOptionSpec)
 *   imports all options from the other option spec into this option spec
 */

#pragma once

#include <pybind11/pybind11.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace elf {
namespace options {

namespace option_spec_detail {

using json = nlohmann::json;

template <typename T, typename Dummy = void>
struct PythonTypename;

template <>
struct PythonTypename<bool, void> {
  static std::string value() {
    return "bool";
  }
};

template <>
struct PythonTypename<std::string, void> {
  static std::string value() {
    return "str";
  }
};

template <typename T>
struct PythonTypename<std::vector<T>, void> {
  static std::string value() {
    return PythonTypename<T>::value();
  }
};

template <typename T>
struct PythonTypename<
    T,
    typename std::enable_if<std::is_floating_point<T>::value>::type> {
  static std::string value() {
    return "float";
  }
};

template <typename T>
struct PythonTypename<
    T,
    typename std::enable_if<
        std::is_integral<T>::value && !std::is_same<T, bool>::value>::type> {
  static std::string value() {
    return "int";
  }
};

class OptionBase {
 public:
  OptionBase(std::string name, const std::type_index& typeIndex)
      : name_(std::move(name)), typeIndex_(typeIndex) {}

  virtual ~OptionBase() {}

  const std::string& getName() const {
    return name_;
  }

  void addPrefixSuffixToName(
      const std::string& prefix,
      const std::string& suffix) {
    name_ = prefix + name_ + suffix;
  }

  template <typename T>
  void checkType() const {
    if (std::type_index(typeid(T)) != typeIndex_) {
      // TODO: Should we be throwing an exception (ssengupta@fb)
      throw std::runtime_error(
          name_ + " is not of type " + typeid(T).name() + "!");
    }
  }

  template <typename T>
  T fromJSON(const json& j) const {
    checkType<T>();
    return j;
  }

  virtual json getPythonArgparseOptionsAsJSON() const = 0;

 protected:
  std::string name_;

  std::type_index typeIndex_;
};

template <typename T>
class OptionBaseTyped : public OptionBase {
 public:
  OptionBaseTyped(std::string name, std::string help)
      : OptionBase(std::move(name), std::type_index(typeid(T))),
        help_(std::move(help)),
        hasDefaultValue_(false),
        defaultValue_() {}

  OptionBaseTyped(std::string name, std::string help, T defaultValue)
      : OptionBase(std::move(name), std::type_index(typeid(T))),
        help_(std::move(help)),
        hasDefaultValue_(true),
        defaultValue_(std::move(defaultValue)) {}

  json getPythonArgparseOptionsAsJSON() const override {
    json args = {"--" + getName()};
    json kwargs = {{"type", PythonTypename<T>::value()},
                   {"help", help_},
                   {"required", !hasDefaultValue_},
                   {"dest", getName()}};
    if (hasDefaultValue_) {
      kwargs["default"] = defaultValue_;
    }
    return {{"args", args}, {"kwargs", kwargs}};
  }

 protected:
  std::string help_;

  bool hasDefaultValue_;

  T defaultValue_;
};

/**
 * This additional level of inheritance is needed because we cannot partially
 * specialize member functions (needed for vector types).
 */
template <typename T>
class Option : public OptionBaseTyped<T> {
 public:
  template <typename... Args>
  Option(Args&&... args) : OptionBaseTyped<T>(std::forward<Args>(args)...) {}
};

template <>
class Option<bool> : public OptionBaseTyped<bool> {
  using T = bool;

 public:
  template <typename... Args>
  Option(Args&&... args) : OptionBaseTyped<T>(std::forward<Args>(args)...) {}

  virtual json getPythonArgparseOptionsAsJSON() const override {
    json ret = OptionBaseTyped<T>::getPythonArgparseOptionsAsJSON();
    ret["kwargs"].erase("type");
    ret["kwargs"].erase("default");
    ret["kwargs"].erase("required");
    bool flip = hasDefaultValue_ && defaultValue_;
    ret["kwargs"]["action"] = flip ? "store_false" : "store_true";
    if (flip) {
      ret["args"] = json::array({"--no_" + getName()});
    }
    return ret;
  }
};

template <typename ItemT>
class Option<std::vector<ItemT>> : public OptionBaseTyped<std::vector<ItemT>> {
  using T = std::vector<ItemT>;

 public:
  template <typename... Args>
  Option(Args&&... args) : OptionBaseTyped<T>(std::forward<Args>(args)...) {}

  virtual json getPythonArgparseOptionsAsJSON() const override {
    json ret = OptionBaseTyped<T>::getPythonArgparseOptionsAsJSON();
    ret["kwargs"]["nargs"] = "*";
    return ret;
  }
};

} // namespace option_spec_detail

class OptionSpec {
  friend class OptionMap;

  using json = nlohmann::json;

 public:
  static void registerPy(pybind11::module& m);

  template <typename T>
  bool addOption(std::string optionName, std::string help) {
    std::shared_ptr<option_spec_detail::OptionBase> option(
        new option_spec_detail::Option<T>(optionName, std::move(help)));
    return optionSpecMap_.emplace(std::move(optionName), std::move(option))
        .second;
  }

  template <typename T>
  bool addOption(std::string optionName, std::string help, T defaultValue) {
    std::shared_ptr<option_spec_detail::OptionBase> option(
        new option_spec_detail::Option<T>(
            optionName, std::move(help), std::move(defaultValue)));
    return optionSpecMap_.emplace(std::move(optionName), std::move(option))
        .second;
  }

  std::vector<std::string> getOptionNames() const {
    std::vector<std::string> names;
    for (const auto& elem : optionSpecMap_) {
      names.emplace_back(elem.first);
    }
    return names;
  }

  json getPythonArgparseOptionsAsJSON() const {
    json arr = json::array();
    for (const auto& item : optionSpecMap_) {
      arr.emplace_back(item.second->getPythonArgparseOptionsAsJSON());
    }
    return arr;
  }

  std::string getPythonArgparseOptionsAsJSONString() const {
    return getPythonArgparseOptionsAsJSON().dump();
  }

  void merge(const OptionSpec& other) {
    optionSpecMap_.insert(
        other.optionSpecMap_.begin(), other.optionSpecMap_.end());
  }

  void addPrefixSuffixToOptionNames(
      const std::string& prefix,
      const std::string& suffix) {
    auto newOptionSpecMap_ = decltype(optionSpecMap_)();
    for (const auto& elem : optionSpecMap_) {
      elem.second->addPrefixSuffixToName(prefix, suffix);
      newOptionSpecMap_.emplace(prefix + elem.first + suffix, elem.second);
    }
    optionSpecMap_ = std::move(newOptionSpecMap_);
  }

 private:
  const option_spec_detail::OptionBase& getOptionInfo(
      const std::string& optionName) const {
    auto it = optionSpecMap_.find(optionName);
    if (it == optionSpecMap_.end()) {
      throw std::runtime_error("No option with name " + optionName + "!");
    }
    return *(it->second);
  }

  std::unordered_map<
      std::string,
      std::shared_ptr<option_spec_detail::OptionBase>>
      optionSpecMap_;
};

} // namespace options
} // namespace elf
