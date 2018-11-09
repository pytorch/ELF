#!/bin/bash

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

LOAD0=$1
shift

LOAD1=$1
shift

BATCHSIZE=$1
shift

NUM_ROLLOUTS=$1
shift

BATCHSIZE2=$1
shift

NUM_ROLLOUTS2=$1
shift

GPU=$1
shift

DIM=224
NUM_BLOCK=20

game=elfgames.tasks.game model=df_pred model_file=elfgames.tasks.df_model3 python3 selfplay.py \
    --mode selfplay --selfplay_timeout_usec 10 \
    --batchsize $BATCHSIZE --mcts_rollout_per_batch $BATCHSIZE \
    --num_games 1 --keys_in_reply V rv --port 2341 --server_id myserver \
    --mcts_threads 2 --mcts_rollout_per_thread $NUM_ROLLOUTS \
    --use_mcts --use_mcts_ai2 --mcts_use_prior \
    --mcts_persistent_tree --mcts_puct 1.5 \
    --batchsize2 $BATCHSIZE2 --white_mcts_rollout_per_batch $BATCHSIZE2 \
    --white_mcts_rollout_per_thread $NUM_ROLLOUTS2 \
    --eval_model_pair loaded \
    --policy_distri_cutoff 0 \
    --mcts_virtual_loss 1 --mcts_epsilon 0.0 --mcts_alpha 0.00 \
    --resign_thres 0.05 \
    --num_block0 $NUM_BLOCK --dim0 $DIM \
    --num_block1 $NUM_BLOCK --dim1 $DIM \
    --no_check_loaded_options0 \
    --no_check_loaded_options1 \
    --verbose \
    --gpu $GPU \
    --load0 $LOAD0 \
    --load1 $LOAD1 \
    --use_fp160 --use_fp161 \
    --gpu $GPU \
    --replace_prefix0 resnet.module,resnet --replace_prefix1 resnet.module,resnet \
    "$@"
