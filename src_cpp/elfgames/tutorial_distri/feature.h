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

class Feature {
 public:
  Feature(const GameOptions &options) 
    : options_(options) {
  }

  void registerExtractor(int batchsize, elf::Extractor& e) {
    // Register multiple fields.
    e.addField<float>("s").addExtents(batchsize, {batchsize, options_.input_dim});
    e.addField<int64_t>("a").addExtent(batchsize);
    e.addField<float>({"V"}).addExtent(batchsize);
    e.addField<float>({"pi"})
        .addExtents(batchsize, {batchsize, options_.num_action});

    /*
    e.addClass<GoReply>()
        .addFunction<int64_t>("a", ReplyAction)
        .addFunction<float>("pi", ReplyPolicy)
        .addFunction<float>("V", ReplyValue)
        .addFunction<int64_t>("rv", ReplyVersion)
        .addFunction<uint64_t>("rhash", ReplyHash);
     */
  }

 private:
  const GameOptions options_;
};
