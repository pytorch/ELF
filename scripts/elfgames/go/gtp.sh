#!/bin/bash

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

MODEL=$1
shift

game=elfgames.go.game model=df_pred model_file=elfgames.go.df_model3 python3 df_console.py --mode online --keys_in_reply V rv \
    --use_mcts --mcts_verbose_time --mcts_use_prior --mcts_persistent_tree --load $MODEL \
    --server_addr localhost --port 1234 \
    --replace_prefix resnet.module,resnet init_conv.module,init_conv \
    --no_check_loaded_options \
    --no_parameter_print \
    "$@"
