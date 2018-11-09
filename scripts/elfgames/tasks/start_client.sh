#!/bin/bash

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

echo $PYTHONPATH $SLURMD_NODENAME $CUDA_VISIBLE_DEVICES

root=./myserver game=elfgames.tasks.game model=df_pred model_file=elfgames.tasks.df_model3 \
stdbuf -o 0 -e 0 python -v ./selfplay.py \
    --T 1    --batchsize 128 \
    --dim0 224    --dim1 224    --gpu 0 \
    --keys_in_reply V rv    --mcts_alpha 0.03 \
    --mcts_epsilon 0.25    --mcts_persistent_tree \
    --mcts_puct 0.85    --mcts_rollout_per_thread 200 \
    --mcts_threads 8    --mcts_use_prior \
    --mcts_virtual_loss 5   --mode selfplay \
    --num_block0 20    --num_block1 20 \
    --num_games 32    --ply_pass_enabled 160 \
    --policy_distri_cutoff 30    --policy_distri_training_for_all \
    --port 1234 \
    --no_check_loaded_options0    --no_check_loaded_options1 \
    --replace_prefix0 resnet.module,resnet init_conv.module,init_conv\
    --replace_prefix1 resnet.module,resnet init_conv.module,init_conv\
    --resign_thres 0.0    --selfplay_timeout_usec 10 \
    --server_id myserver    --use_mcts \
    --use_fp160 --use_fp161 \
    --use_mcts_ai2 --verbose
