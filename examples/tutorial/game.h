#pragma once
#include <string>
#include <iostream>
#include <iomanip>

struct State {
  int id;
  int value;
  int seq = 0;
  int reply;

  State() {}
  State(const State&) = delete;

  void dumpState(int *state) const {
    // Dump the last n state.
    // cout << "Dump state for id=" << id << ", seq=" << seq << endl;
    *state = value;
  }

  void loadReply(const int* a) {
    // cout << "[" << hex << this << dec << "] load reply for id=" << id << ",
    // seq=" << seq << ", a=" << *a << endl;
    reply = *a;
  }
};

namespace game {

class World {
 public:
   void setIdx(int idx) {
     s_.id = idx;
     s_.value = idx + s_.seq;
   }

   void step(bool success) {
     if (success) {
       if (s_.reply != 2 * (s_.id + s_.seq) + 1) {
         std::cout << "Error: [" << std::hex << &s_ << std::dec << "] client "
           << s_.id << " return from #" << s_.seq
           << ", value: " << s_.value
           << ", reply = " << s_.reply 
           << std::endl;
       }
     } else {
       // std::cout << "client " << idx_ << " return from #" << j << "
       // failed.";
     }
     s_.seq ++;
   }

   State &s() { return s_; }
   const State &s() const { return s_; }

 private:
  State s_;
};

void getStateFeature(const World &w, int *state) {
  w.s().dumpState(state);
}

void setReply(World &w, const int *reply) {
  w.s().loadReply(reply);
}

} // namespace game
