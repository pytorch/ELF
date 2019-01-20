#include "hist.h"

namespace elf {

namespace decorator {

class FrameStacking {
 public:
  using Hist = elf::HistT<std::vector<float>>;

  FrameStacking(size_t framestack, size_t dim, float default_value = 0.0f)
      : frame_stack_(framestack), dim_(dim), trait_(dim, default_value), hist_(framestack) {
          reset();
      }

  void reset() {
    hist_.reset([&](std::vector<float> &v) { trait_.Initialize(v); });
  }

  void feedObs(std::vector<float> &&f) {
    assert(f.size() == dim_);
    hist_.push(std::move(f));
  }

  std::vector<float> feature() const {
    std::vector<float> v(dim_ * frame_stack_, trait_.getUndefValue());
    float *p = &v[0];
    hist_.getInterval().backward([&](const std::vector<float> &v) { p += trait_.Extract(v, p); });
    return v;
  }

  const Hist& hist() const { return hist_; }

 private:
  size_t frame_stack_;
  size_t dim_;
  elf::HistTrait<std::vector<float>> trait_;
  Hist hist_;
};

template <typename Reply>
class ShortReplayBuffer {
 public:
  using Hist = elf::HistT<Reply>;

  ShortReplayBuffer(int T) : hist_(T) {
  }

  void reset(std::function<void (Reply &)> resetter = nullptr) {
    hist_.reset(resetter);
    last_step_ = 0;
    curr_step_ = 0; 
  }

  void feedReplay(Reply &&reply) {
    // std::cout << "feedReplay: " << f.size() << std::endl;
    hist_.push(std::move(reply));
    curr_step_ ++;
  }

  bool needSendReplay() {
    if (isFull() && (curr_step_ - last_step_ == hist_.maxlen())) {
      // std::cout << "last_step: " << last_step_ << ", curr_step: " << curr_step_ << std::endl;
      last_step_ = curr_step_ - 1;
      return true;
    } else return false;
  }

  bool isFull() const { return hist_.isFull(); }

  const Hist& hist() const { return hist_; }

 private:
  size_t last_step_ = 0;
  size_t curr_step_ = 0; 
  Hist hist_;
};

}  // namespace decorator

}  // namespace elf
