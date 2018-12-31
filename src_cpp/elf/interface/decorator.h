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
    hist_.extractReverse<float>(CURR_SIZE, &v[0], [&](const std::vector<float> &v, float *s) { return trait_.Extract(v, s); });
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
  using Hist = elf::HistT<std::vector<float>>;
  using HistR = elf::HistT<Reply>;

  ShortReplayBuffer(int T, int dim) 
     : dim_(dim), trait_(dim, 0), hist_(T), hist_reply_(T) {
  }

  void reset(std::function<void (Reply &)> resetter = nullptr) {
    hist_.reset([&](std::vector<float> &v) { return trait_.Initialize(v); });
    hist_reply_.reset(resetter);
    last_step_ = 0;
    curr_step_ = 0; 
  }

  void feedReplay(std::vector<float> &&f, Reply &&reply) {
    // std::cout << "feedReplay: " << f.size() << std::endl;
    assert(f.size() == dim_);
    hist_.push(std::move(f));
    hist_reply_.push(std::move(reply));
    curr_step_ ++;
  }

  bool needSendReplay() {
    if (isFull() && (curr_step_ - last_step_ == hist_.maxlen())) {
      last_step_ = curr_step_ - 1;
      return true;
    } else return false;
  }

  bool isFull() const { return hist_.isFull() && hist_reply_.isFull(); }

  void getFeatureForward(float *s) const {
    hist_.extractForward<float>(FULL_ONLY, s, [&](const std::vector<float> &v, float *s) { return trait_.Extract(v, s); });
  }
  void getFeatureReverse(float *s) const {
    hist_.extractReverse<float>(FULL_ONLY, s, [&](const std::vector<float> &v, float *s) { return trait_.Extract(v, s); });
  }

  const Hist& hist() const { return hist_; }
  const HistR& histReply() const { return hist_reply_; }

 private:
  size_t last_step_ = 0;
  size_t curr_step_ = 0; 
  size_t dim_;
  elf::HistTrait<std::vector<float>> trait_;
  Hist hist_;
  HistR hist_reply_;
};

}  // namespace decorator

}  // namespace elf
