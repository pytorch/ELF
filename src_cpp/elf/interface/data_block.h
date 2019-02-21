#pragma once

#include <vector>
#include "extractor.h"

namespace {

inline int getProduct(const std::vector<int>& nums) {
  int prod = 1;
  for (auto v : nums) {
    prod *= v;
  }
  return prod;
}

inline std::vector<int> pushLeft(int left, const std::vector<int>& nums) {
  std::vector<int> vec;
  vec.push_back(left);
  for (auto v : nums) {
    vec.push_back(v);
  }
  return vec;
}

} // unnamed namespace

namespace elf {

template <typename T>
class NdArray {
 public:
  NdArray(std::vector<int> sizes, T val)
      : sizes_(sizes),
        buffer_(getProduct(sizes_), val),
        dims_(sizes_.size())
  {}

  void extract(T* dest) const {
    std::copy(buffer_.begin(), buffer_.end(), dest);
  }

  void reply(const T* src) {
    std::copy(src, src + buffer_.size(), buffer_.begin());
  }

  T& at(int idx) {
    assert(idx < (int)buffer_.size());
    return buffer_[idx];
  }

  const T& at(int idx) const {
    assert(idx < (int)buffer_.size());
    return buffer_[idx];
  }

  T& at(const std::vector<int>& indices) {
    assert((int)indices.size() == dims_);
    int idx = 0;
    int offset = 1;
    for (int i = dims_ - 1; i >= 0; --i) {
      assert(indices[i] < sizes_[i]);
      idx += indices[i] * offset;
      offset *= sizes_[i];
    }
    assert(idx < (int)buffer_.size());
    return buffer_[idx];
  }

  void fill(T val) {
    std::fill(buffer_.begin(), buffer_.end(), val);
  }

  const T* getRawData() const {
    return buffer_.data();
  }

  const std::vector<T>& getData() const {
    return buffer_;
  }

 private:
  std::vector<int> sizes_;
  std::vector<T> buffer_;
  int dims_;
};

template <typename T>
class DataBlock {
 public:
  DataBlock(std::string name, int histLen, std::initializer_list<int> sizes, T val)
      : name_(name),
        histName_("hist_" + name),
        needHist_(histLen > 0),
        histLen_(histLen),
        sizes_(sizes),
        dataSize_(getProduct(sizes_)),
        // currentIdx_(0),
        nextHistIdx_(0),
        data_(sizes_, val)
  {
    if (histLen_ <= 0) {
      histLen_ = 1;
    }
    histData_ = std::vector<NdArray<T>>(histLen_, NdArray(sizes_, val));
  }

  DataBlock(const DataBlock&) = delete;
  DataBlock& operator=(const DataBlock&) = delete;

  const std::string& getName(bool hist) const {
    if (hist) {
      return histName_;
    } else {
      return name_;
    }
  }

  void registerData(int bsize, elf::Extractor& e, bool needSend, bool needReply) const {
    auto fsize = pushLeft(bsize, sizes_);
    if (needSend) {
      std::cout << "registering send: " << name_ << std::endl;
      e.addField<T>(name_)
          .addExtents(bsize, fsize)
          .template addFunction<DataBlock<T>>(&DataBlock<T>::sendData);
    }

    if (needReply) {
      std::cout << "registering reply: " << name_ << std::endl;
      e.addField<T>(name_)
          .addExtents(bsize, fsize)
          .template addFunction<DataBlock<T>>(&DataBlock<T>::replyData);
    }

    if (needHist_) {
      std::cout << "registering send history: " << histName_ << std::endl;
      auto fsize = pushLeft(bsize, pushLeft(histLen_, sizes_));
      e.addField<T>(histName_)
          .addExtents(bsize, fsize)
          .template addFunction<DataBlock<T>>(&DataBlock<T>::sendHistData);
    }
  }

  size_t size() {
    return dataSize_;
  }

  std::vector<int> sizes() {
    return sizes_;
  }

  T& at(int index) {
    return data_.at(index);
  }

  const T& at(int index) const {
    return data_.at(index);
  }

  T& at(const std::vector<int>& indices) {
    return data_.at(indices);
  }

  void fill(T val) {
    data_.fill(val);
  }

  void copyFrom(const DataBlock& src) {
    replyData(src.data_.getRawData());
  }

  const std::vector<T>& getData() const {
    return data_.getData();
  }

  void sendData(T* buffer) const {
    data_.extract(buffer);
  }

  void replyData(const T* buffer) {
    data_.reply(buffer);
  }

  void sendHistData(T* buffer) const {
    // std::cout << "extracting " << histName_ << std::endl;
    // if (currentIdx_ != 0) {
    //   std::cout << "current idx: " << currentIdx_ << std::endl;
    //   std::cout << "hist len: " << histLen_ << std::endl;
    // }
    assert(nextHistIdx_ == 0);
    int offset = 0;

    for (int i = 0; i < histLen_; ++i) {
      histData_[i].extract(buffer + offset);
      offset += dataSize_;
    }
  }

  int pushDataToHist() {
    assert(needHist_);
    int pushedIdx = nextHistIdx_;
    histData_[pushedIdx].reply(data_.getRawData());
    nextHistIdx_ = (nextHistIdx_ + 1) % histLen_;
    return pushedIdx;
  }

  void print() {
    for (int i = 0; i < histLen_; ++i) {
      std::cout << "hist t = " << i << std::endl;
      for (auto v : histData_[i].getData()) {
        std::cout << v << ", ";
      }
      std::cout << std::endl;
    }
  }

 private:
  std::string name_;
  std::string histName_;
  bool needHist_;
  int histLen_;
  std::vector<int> sizes_;  // exclude hist dim
  int dataSize_;

  // manages history
  // int currentIdx_;
  int nextHistIdx_;
  NdArray<T> data_;
  std::vector<NdArray<T>> histData_;
};

} // elf
