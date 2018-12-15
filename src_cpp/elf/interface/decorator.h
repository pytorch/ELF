#include "elf/base/hist.h"

namespace elf {

namespace interface {

class FrameStacking {
 public:
  using Hist = elf::HistT<std::vector<float>>;

  FrameStacking(size_t framestack, size_t dim)
      : frame_stack_(framestack), dim_(dim), trait_(dim, 0), hist_(framestack) {
          reset();
      }

  void reset() {
    hist_.reset([&](std::vector<float> &v) { trait_.Initialize(v); });
  }

  void feedObs(std::vector<float> &&f) {
    assert(f.size() == dim_);
    hist_.push(std::move(f));
  }

  void getFeature(float *s) const {
    hist_.extract<float>(s, [&](const std::vector<float> &v, float *s) { return trait_.Extract(v, s); });
  }

  std::vector<float> feature() const {
    size_t total_size = hist_.accumulate([](const std::vector<float> &v) { return v.size(); });
    assert(total_size == dim_ * frame_stack_);
    std::vector<float> v(total_size);
    getFeature(&v[0]);
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

  void reset() {
    hist_.reset([&](std::vector<float> &v) { return trait_.Initialize(v); });
    hist_reply_.reset(nullptr);
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
    if (curr_step_ - last_step_ == hist_.size()) {
      last_step_ = curr_step_ - 1;
      return true;
    } else return false;
  }

  void getFeature(float *s) const {
    hist_.extract<float>(s, [&](const std::vector<float> &v, float *s) { return trait_.Extract(v, s); });
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

}  // namespace interface

}  // namespace elf
