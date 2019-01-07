#!/bin/bash

./setaddr.sh

touch ../../../log.log

sleep 1
rm ../../../log.log
# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

save=./myserver game=elfgames.tasks.game model=df_kl model_file=elfgames.tasks.df_model3 \
    stdbuf -o 0 -e 0 python -u ./train.py \
    --mode train    --batchsize 64 \
    --num_games 64 --keys_in_reply V \
    --T 1    --use_data_parallel \
    --num_minibatch 10    --num_episode 200 \
    --mcts_threads 1    --mcts_rollout_per_thread 20 \
    --keep_prev_selfplay    --keep_prev_selfplay \
    --use_mcts     --use_mcts_ai2 \
    --mcts_persistent_tree    --mcts_use_prior \
    --mcts_virtual_loss 5     --mcts_epsilon 0.25 \
    --mcts_alpha 0.03     --mcts_puct 0.85 \
    --resign_thres 0.01    --gpu 1 \
    --server_id myserver     --eval_num_games 400 \
    --eval_winrate_thres 0.55     --port 1234 \
    --q_min_size 10     --q_max_size 40 \
    --save_first     \
    --num_block 2     --dim 19 \
    --weight_decay 0.0002    --opt_method sgd \
    --bn_momentum=0 --num_cooldown=50 \
    --expected_num_client 3 \
    --selfplay_init_num 0 --selfplay_update_num 0 \
    --eval_num_games 0 --selfplay_async \
    --lr 0.01    --momentum 0.9     1>> ../../../log.log 2>&1 


cd ../../..
#./allkill.sh
