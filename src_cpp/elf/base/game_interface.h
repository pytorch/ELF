#pragma once

#include "game_base.h"
#include "options.h"
#include "sharedmem_data.h"
#include "extractor.h"

namespace elf {

class GCInterface {
 public:
  GCInterface(const Options& options) 
    : options_(options) {}

  const Options& options() const {
    return options_;
  }

  // For Python side.
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual SharedMemData* wait(int time_usec = 0) = 0;
  virtual void step(comm::ReplyStatus success = comm::SUCCESS) = 0;
  virtual SharedMemData& allocateSharedMem(
      const SharedMemOptions& options,
      const std::vector<std::string>& keys) = 0;

  virtual GameClientInterface* getClient() = 0;
  virtual Extractor& getExtractor() = 0;

  virtual const game::Base* getGameC(int /*game_idx*/) const {
    return nullptr;
  }
  virtual elf::game::Base* getGame(int /*game_idx*/) {
    return nullptr;
  }

 protected:
  Options options_;
};

} // namespace elf
