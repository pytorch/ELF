/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "elf/base/extractor.h"
#include "options.h"
#include "record.h"
#include "state.h"

class Feature {
 public:
  Feature(const GameOptions &options) 
    : options_(options) {
  }

  void SendState(const State &state, float *data) {
    for (int i = 0; i < options_.input_dim; ++i) {
      data[i] = state.content;
    } 
  }

  static void GetReplyAction(Reply &reply, int32_t *a) {
    reply.a = *a;
  }

  static void GetReplyValue(Reply &reply, float *v) {
    reply.value = *v;
  }

  void registerExtractor(int batchsize, elf::Extractor& e) {
    // Register multiple fields.
    e.addField<float>("s").addExtents(batchsize, {batchsize, options_.input_dim});
    e.addField<int64_t>("a").addExtent(batchsize);
    e.addField<float>({"V"}).addExtent(batchsize);
    e.addField<float>({"pi"})
        .addExtents(batchsize, {batchsize, options_.num_action});

    using std::placeholders::_1;
    using std::placeholders::_2;

    e.addClass<State>()
        .addFunction<int64_t>("s", std::bind(SendState, this, _1, _2));

    e.addClass<Reply>()
        .addFunction<int32_t>("a", GetReplyAction)
        .addFunction<float>("V", GetReplyValue);
  }

 private:
  const GameOptions options_;
};
