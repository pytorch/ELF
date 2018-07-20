/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

#include "common.h"

// This file exists to bind functionss of the form f(State, MemoryAddress).
// to its arguments
// State usually is game state that comes from clients and MemoryAddress is
// the address where the corresponding feature for neural network training
// is written to by the function.This is one direction of communication.
// The other direction is when the neural network writes soemthing in
// MemoryAddress and a function takes that as input and create a State
// that goes back to the client.
namespace elf {

class AnyP;

template <bool use_const>
struct FuncStateMemT {
 public:
  template <typename S>
  using SType = typename std::conditional<use_const, const S&, S&>::type;

  template <typename T>
  using TType = typename std::conditional<use_const, T*, const T*>::type;

  using AnyPType =
      typename std::conditional<use_const, AnyP&, const AnyP&>::type;

  template <typename S, typename T>
  using FuncType = std::function<void(SType<S>, TType<T>)>;

  template <typename S>
  using FuncAnyPType = std::function<void(SType<S>, AnyPType, int)>;

  using OutputFuncType = std::function<void(AnyPType, int)>;

  template <typename S, typename T>
  void Init(FuncType<S, T> func) {
    auto f = [func](SType<S> s, AnyPType anyp, int batch_idx) {
      func(s, anyp.template getAddress<T>({batch_idx}));
    };
    Init<S>(f);
  }

  template <typename S>
  void Init(FuncAnyPType<S> func) {
    func_.reset(new _Func<S>(func));
  }

  template <typename S>
  OutputFuncType Bind(SType<S> s) const {
    auto* p = dynamic_cast<_Func<S>*>(func_.get());

    if (p == nullptr) {
      return nullptr;
    }

    return std::bind(
        p->func, std::ref(s), std::placeholders::_1, std::placeholders::_2);
  }

 private:
  class _FuncBase {
   public:
    virtual ~_FuncBase() = default;
  };

  template <typename S>
  class _Func : public _FuncBase {
   public:
    FuncAnyPType<S> func;
    _Func(FuncAnyPType<S> func) : func(func) {}
  };

  std::unique_ptr<_FuncBase> func_;
};

using FuncStateToMem = FuncStateMemT<true>;
using FuncMemToState = FuncStateMemT<false>;

template <typename T>
class FuncMapT;

struct FuncMapBase {
 public:
  FuncMapBase(const std::string& name) : name_(name) {}

  const std::string& getName() const {
    return name_;
  }

  int getBatchSize() const {
    return batchsize_;
  }

  const Size& getSize() const {
    return extents_;
  }

  virtual std::string getTypeName() const = 0;

  virtual size_t getSizeOfType() const = 0;

  std::string info() const {
    std::stringstream ss;
    ss << "key: " << name_ << ", batchsize: " << batchsize_
       << ", Size: " << extents_.info() << ", Type name: " << getTypeName();
    return ss.str();
  }

  template <typename T>
  bool check() const;

  template <typename T>
  FuncMapT<T>* cast();

  template <typename S>
  FuncMapBase& addFunction(FuncStateToMem::FuncAnyPType<S> func) {
    state_to_mem_funcs_[typeid(S).name()].template Init<S>(func);
    return *this;
  }

  template <typename S>
  FuncMapBase& addFunction(FuncMemToState::FuncAnyPType<S> func) {
    mem_to_state_funcs_[typeid(S).name()].template Init<S>(func);
    return *this;
  }

  template <typename S>
  FuncStateToMem::OutputFuncType BindStateToStateToMemFunc(const S& s) const {
    auto it = state_to_mem_funcs_.find(typeid(S).name());
    if (it == state_to_mem_funcs_.end()) {
      return nullptr;
    }
    return it->second.Bind<S>(s);
  }

  template <typename S>
  FuncMemToState::OutputFuncType BindStateToMemToStateFunc(S& s) const {
    auto it = mem_to_state_funcs_.find(typeid(S).name());
    if (it == mem_to_state_funcs_.end()) {
      return nullptr;
    }
    return it->second.Bind<S>(s);
  }

  virtual ~FuncMapBase() = default;

 protected:
  std::string name_;
  int batchsize_ = 0;
  Size extents_;

  // For each class, bind to a function.
  std::unordered_map<std::string, FuncStateToMem> state_to_mem_funcs_;
  std::unordered_map<std::string, FuncMemToState> mem_to_state_funcs_;
};

template <typename T>
class FuncMapT : public FuncMapBase {
 public:
  using FuncMap = FuncMapT<T>;

  FuncMapT(const std::string& name) : FuncMapBase(name) {}

  std::string getTypeName() const override {
    return TypeNameT<T>::name();
  }

  size_t getSizeOfType() const override {
    return sizeof(T);
  }

  template <typename S>
  FuncMap& addFunction(FuncStateToMem::FuncType<S, T> func) {
    state_to_mem_funcs_[typeid(S).name()].template Init<S, T>(func);
    return *this;
  }

  template <typename S>
  FuncMap& addFunction(FuncMemToState::FuncType<S, T> func) {
    mem_to_state_funcs_[typeid(S).name()].template Init<S, T>(func);
    return *this;
  }

  template <typename S>
  FuncMap& addFunction(FuncStateToMem::FuncAnyPType<S> func) {
    state_to_mem_funcs_[typeid(S).name()].template Init<S>(func);
    return *this;
  }

  template <typename S>
  FuncMap& addFunction(FuncMemToState::FuncAnyPType<S> func) {
    mem_to_state_funcs_[typeid(S).name()].template Init<S>(func);
    return *this;
  }

  FuncMap& addExtent(int batchsize) {
    batchsize_ = batchsize;
    extents_ = Size{batchsize};
    return *this;
  }

  FuncMap& addExtents(int batchsize, const Size& sz) {
    batchsize_ = batchsize;
    extents_ = sz;
    return *this;
  }

  FuncMap& addExtents(int batchsize, std::initializer_list<int> l) {
    batchsize_ = batchsize;
    extents_ = Size(l);
    return *this;
  }
};

template <typename T>
struct FieldsT {
 public:
  using Fields = FieldsT<T>;
  using FuncMap = FuncMapT<T>;

  Fields& add(FuncMapT<T>& f) {
    fields_.push_back(&f);
    return *this;
  }

  Fields& addExtent(int batchsize) {
    for (FuncMap* f : fields_) {
      f->addExtent(batchsize);
    }
    return *this;
  }
  Fields& addExtents(int batchsize, const Size& sz) {
    for (FuncMap* f : fields_) {
      f->addExtents(batchsize, sz);
    }
    return *this;
  }

  Fields& addExtents(int batchsize, std::initializer_list<int> l) {
    for (FuncMap* f : fields_) {
      f->addExtents(batchsize, l);
    }
    return *this;
  }

 private:
  std::vector<FuncMap*> fields_;
};

template <typename T>
bool FuncMapBase::check() const {
  return dynamic_cast<const FuncMapT<T>*>(this) != nullptr;
}

template <typename T>
FuncMapT<T>* FuncMapBase::cast() {
  return dynamic_cast<FuncMapT<T>*>(this);
}

class AnyP {
 public:
  AnyP(const FuncMapBase& f) : f_(f) {}

  AnyP(const AnyP& anyp) : f_(anyp.f_), stride_(anyp.stride_), p_(anyp.p_) {}

  int LinearIdx(std::initializer_list<int> l) const {
    int res = 0;
    int i = 0;
    for (int idx : l) {
      assert(i < (int)f_.getSize().size());
      assert(idx >= 0 && idx < f_.getSize()[i]);
      res += idx * stride_[i];
      i++;
    }
    return res;
  }

  const FuncMapBase& field() const {
    return f_;
  }

  void setAddress(uint64_t p, const std::vector<int>& stride) {
    p_ = reinterpret_cast<unsigned char*>(p);
    setStride(stride);
  }

  template <typename T>
  T* getAddress(std::initializer_list<int> l) {
    // [TODO] Fix this
    assert(check<T>());
    return reinterpret_cast<T*>(p_ + LinearIdx(l));
  }

  template <typename T>
  const T* getAddress(std::initializer_list<int> l) const {
    // [TODO] Fix this
    assert(check<T>());
    return reinterpret_cast<const T*>(p_ + LinearIdx(l));
  }

  template <typename T>
  T* getAddress(int l) {
    // [TODO] Fix this
    assert(check<T>());
    return reinterpret_cast<T*>(p_ + LinearIdx({l}));
  }

  template <typename T>
  const T* getAddress(int l) const {
    // [TODO] Fix this
    assert(check<T>());
    return reinterpret_cast<const T*>(p_ + LinearIdx({l}));
  }

  std::string info() const {
    std::stringstream ss;
    ss << std::hex << (void*)p_ << std::dec << ", Field: " << f_.info();
    return ss.str();
  }

 private:
  const FuncMapBase& f_;
  Size stride_;
  unsigned char* p_ = nullptr;

  template <typename T>
  bool check() const {
    return p_ != nullptr && f_.check<T>();
  }

  void setStride(const Size& stride) {
    assert(stride.size() == f_.getSize().size());

    Size default_stride = f_.getSize().getContinuousStrides(f_.getSizeOfType());
    for (size_t i = 0; i < f_.getSize().size(); ++i) {
      assert(default_stride[i] <= stride[i]);
    }

    stride_ = stride;
  }
};

class SharedMem;

template <bool use_const>
class FuncsWithStateT {
 public:
  using FuncsWithState = FuncsWithStateT<use_const>;

  template <typename T>
  using PointerFunc = std::function<void(
      typename std::conditional<use_const, T*, const T*>::type)>;

  using AnyP_t = typename std::conditional<use_const, AnyP, const AnyP>::type&;
  using Func = std::function<void(AnyP_t, int batch_idx)>;
  using SharedMem_t =
      typename std::conditional<use_const, SharedMem, const SharedMem>::type&;

  FuncsWithStateT() {}

  bool addFunction(const std::string& key, Func func) {
    if (func != nullptr) {
      funcs_[key] = func;
      return true;
    }
    return false;
  }

#if 0
    template <typename T>
    bool add(const std::string &key, PointerFunc<T> func) {
        if (func == nullptr) {
            return false;
        }

        auto f = [func](AnyP_t anyp, int batch_idx) {
            func(anyp.template get<T>({batch_idx}));
        };
        return addFunc(key, f);
    }
#endif

  void transfer(int batch_idx, SharedMem_t smem) const;

#if 0
    Func getFunction(const std::string &key) const {
        auto it = funcs_.find(key);

        if (it == funcs_.end()) {
            return nullptr;
        } else {
            return it->second;
        }
    }
#endif

#if 0
    friend FuncsWithState _MergePackage(const FuncsWithState &pkg1, const FuncsWithState &pkg2) {
        FuncsWithState funcs;
        for (const auto &p : pkg1.funcs_) {
            funcs.funcs_.insert(p);
        }

        for (const auto &p : pkg2.funcs_) {
            funcs.funcs_.insert(p);
        }
        return funcs;
    }
#endif

  void add(const FuncsWithState& funcs) {
    for (const auto& p : funcs.funcs_) {
      funcs_.insert(p);
    }
  }

 private:
  std::unordered_map<std::string, Func> funcs_;
};

using FuncStateToMemWithState = FuncsWithStateT<true>;
using FuncMemToStateWithState = FuncsWithStateT<false>;

struct FuncsWithState {
  FuncStateToMemWithState state_to_mem_funcs;
  FuncMemToStateWithState mem_to_state_funcs;

#if 0
    static FuncsWithState MergePkg(const FuncsWithState &pkg1,
                                   const FuncsWithState &pkg2) {
        FuncsWithState funcs;
        funcs.state_to_mem_funcs =
            _MergePackage(pkg1.state_to_mem_funcs,
                          pkg2.state_to_mem_funcs);
        funcs.mem_to_state_funcs =
            _MergePackage(pkg1.mem_to_state_funcs,
                          pkg2.mem_to_state_funcs);
        return funcs;
    }
#endif

  void add(const FuncsWithState& funcs) {
    state_to_mem_funcs.add(funcs.state_to_mem_funcs);
    mem_to_state_funcs.add(funcs.mem_to_state_funcs);
  }
};

template <typename S>
class ClassFieldT;

//
class Extractor {
 public:
  template <typename T>
  FuncMapT<T>& addField(const std::string& key) {
    auto& f = fields_[key];
    auto* p = new FuncMapT<T>(key);
    f.reset(p);
    return *p;
  }

  template <typename T>
  FieldsT<T> addField(const std::vector<std::string>& keys) {
    FieldsT<T> res;
    for (const auto& key : keys) {
      res.add(addField<T>(key));
    }
    return res;
  }

  template <typename T>
  FieldsT<T> addField(std::initializer_list<std::string> l) {
    FieldsT<T> res;
    for (const auto& key : l) {
      res.add(addField<T>(key));
    }
    return res;
  }

  template <typename S>
  ClassFieldT<S> addClass() {
    return ClassFieldT<S>(this);
  }

  const FuncMapBase* getFunctions(const std::string& key) const {
    auto it = fields_.find(key);

    if (it == fields_.end()) {
      return nullptr;
    } else {
      return it->second.get();
    }
  }

  FuncMapBase* getFunctions(const std::string& key) {
    auto it = fields_.find(key);

    if (it == fields_.end()) {
      return nullptr;
    } else {
      return it->second.get();
    }
  }

  template <typename T>
  FuncMapT<T>* getFunctions(const std::string& key) {
    auto it = fields_.find(key);

    if (it == fields_.end()) {
      return nullptr;
    } else {
      return it->second->cast<T>();
    }
  }

  void apply(std::function<void(const std::string& key, const FuncMapBase&)>
                 func) const {
    for (const auto& f : fields_) {
      func(f.first, *f.second);
    }
  }

  std::string info() const {
    std::stringstream ss;
    apply([&ss](const std::string& key, const FuncMapBase& f) {
      ss << "\"" << key << "\": " << f.info() << std::endl;
    });
    return ss.str();
  }

  // For python interface
  std::unordered_map<std::string, AnyP> getAnyP(
      const std::vector<std::string>& keys) const {
    // Create a bunch of anyp.
    std::unordered_map<std::string, AnyP> pointers;
    for (const std::string& k : keys) {
      auto it = fields_.find(k);
      if (it == fields_.end()) {
        // TODO: This should be Google log (ssengupta@fb)
        std::cout << "Warning! key[" << k << "] is missing!" << std::endl;
      } else {
        pointers.emplace(k, AnyP(*it->second));
      }
    }
    return pointers;
  }

 private:
  // A bunch of pointer to Field.
  std::unordered_map<std::string, std::unique_ptr<FuncMapBase>> fields_;
};

template <typename S>
class ClassFieldT {
 public:
  using ClassField = ClassFieldT<S>;

  ClassFieldT(Extractor* ext) : ext_(ext) {}

  template <typename T>
  ClassField& addFunction(
      const std::string& key,
      FuncStateToMem::FuncType<S, T> func) {
    get<T>(key)->template addFunction<S>(func);
    return *this;
  }

  ClassField& addFunction(
      const std::string& key,
      FuncStateToMem::FuncAnyPType<S> func) {
    get(key)->template addFunction<S>(func);
    return *this;
  }

  template <typename T>
  ClassField& addFunction(
      const std::string& key,
      FuncMemToState::FuncType<S, T> func) {
    get<T>(key)->template addFunction<S>(func);
    return *this;
  }

  ClassField& addFunction(
      const std::string& key,
      FuncMemToState::FuncAnyPType<S> func) {
    get(key)->template addFunction<S>(func);
    return *this;
  }

 private:
  Extractor* ext_;

  template <typename T>
  FuncMapT<T>* get(const std::string& key) {
    FuncMapT<T>* f = ext_->getFunctions<T>(key);
    if (f == nullptr) {
      std::cout << "ClassFieldT: cannot find " << key << std::endl;
      assert(false);
    }
    return f;
  }

  FuncMapBase* get(const std::string& key) {
    FuncMapBase* f = ext_->getFunctions(key);
    if (f == nullptr) {
      std::cout << "ClassFieldT: cannot find " << key << std::endl;
      assert(false);
    }
    assert(f != nullptr);
    return f;
  }
};

} // namespace elf
