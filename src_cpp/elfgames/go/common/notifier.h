#pragma once

#include "../mcts/mcts.h"
#include "elf/ai/tree_search/mcts.h"
#include "go_state_ext.h"
#include "record.h"

class GameNotifierBase {
 public:
  using MCTSResult = elf::ai::tree_search::MCTSResultT<Coord>;
  virtual void OnGameEnd(const GoStateExt&) {}
  virtual void OnStateUpdate(const ThreadState&) {}
  virtual void OnMCTSResult(Coord, const MCTSResult&) {}
};
