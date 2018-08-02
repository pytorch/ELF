#pragma once
#include <string>
#include <vector>

namespace elf {
namespace ai {
namespace tree_search {

using NodeId = int64_t;
const NodeId InvalidNodeId = -1;

struct Score {
  float q;
  float unsigned_q;
  float prior_probability;
  bool first_visit;
};

struct EdgeInfo {
  // From state.
  float prior_probability;
  NodeId child_node;

  // Accumulated reward and #trial.
  float reward;
  int num_visits;
  float virtual_loss;

  EdgeInfo(float probability)
      : prior_probability(probability),
        child_node(InvalidNodeId),
        reward(0),
        num_visits(0),
        virtual_loss(0) {}

  float getQSA() const {
    return reward / num_visits;
  }

  // TODO: What is this function doing (ssengupta@fb.com)
  void checkValid() const {
    if (virtual_loss != 0) {
      // TODO: This should be a Google log (ssengupta@fb)
      std::cout << "Virtual loss is not zero[" << virtual_loss << "]"
                << std::endl;
      std::cout << info(true) << std::endl;
      assert(virtual_loss == 0);
    }
  }

  Score getScore(
      bool flip_q_sign,
      int total_parent_visits,
      float unsigned_default_q) const {
    float r = reward;
    if (flip_q_sign) {
      r = -r;
    }

    // Virtual loss.
    // After flipping, r is the win count (-1 for loss, and +1 for win).
    r -= virtual_loss;
    const int num_visits_with_loss = num_visits + virtual_loss;

    Score s;
    s.q =
        (num_visits_with_loss > 0
             ? r / num_visits_with_loss
             : (flip_q_sign ? -unsigned_default_q : unsigned_default_q));
    s.unsigned_q = (num_visits > 0 ? reward / num_visits : unsigned_default_q);
    s.prior_probability =
        prior_probability / (1 + num_visits) * std::sqrt(total_parent_visits);
    s.first_visit = (num_visits_with_loss == 0);

    return s;
  }

  std::string info(bool verbose = false) const {
    std::stringstream ss;

    if (verbose == false) {
      ss << reward << "/" << num_visits << " (" << getQSA()
         << "), Pr: " << prior_probability << ", child node: " << child_node;
    } else {
      ss << "[" << reward << "/" << num_visits << "]["
         << "vl: " << virtual_loss << "][prob:" << prior_probability
         << "][num_visits:" << num_visits << "]";
    }
    return ss.str();
  }
};

} // namespace tree_search
} // namespace ai
} // namespace elf
