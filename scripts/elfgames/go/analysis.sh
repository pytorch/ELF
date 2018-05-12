#!/bin/bash

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

MODEL=$1
shift

game=elfgames.go.game model=df_pred model_file=elfgames.go.df_model3 python3 selfplay.py --mode selfplay --keys_in_reply V rv \
    --use_mcts --use_mcts_ai2 --mcts_verbose_time --mcts_use_prior --mcts_persistent_tree --load0 $MODEL --load1 $MODEL \
    --selfplay_timeout_usec 10 \
    --server_addr localhost --port 1234 \
    --replace_prefix0 resnet.module,resnet \
    --replace_prefix1 resnet.module,resnet \
    --eval_model_pair loaded \
    --dim0 224 --dim1 224 --num_block0 20 --num_block1 20 \
    --no_check_loaded_options0 --no_check_loaded_options1 \
    --dump_record_prefix tree --num_games 1 \
    "$@"
