#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <random>
#include <vector>

namespace elf {

namespace cs {

using json = nlohmann::json;

class GameInterface {
 public:
   virtual json to_json() const = 0;
   virtual void step() = 0;
   virtual std::unordered_map<std::string, int> getParams() const = 0;
};

class GameFactory {
 public:
   std::function<std::unique_ptr<GameInterface> (const json &, std::mt19937 *)> from_json;
};

}  // namespace cs

}  // namespace elf
