GPU=$1
shift

ROOT=`python3 root_path.py`

DIM=64
NUM_BLOCK=5

ADDR="[`hostname -I | cut -d ' ' -f 1`]"
echo $ADDR

source ../../devmode_set_pythonpath.sh

NUM_ROLLOUTS=200

save=$ROOT/$ADDR game=elfgames.go.server model=df_kl model_file=elfgames.go.df_model3 python3 -u train.py --common.mode train --common.net.server_addr $ADDR --common.base.batchsize 2048 --common.base.num_game_thread 128 \
    --keys_in_reply V --use_data_parallel --num_episode 1000000 --num_minibatch 50  --bn_momentum 0.1 --num_cooldown 50 --selfplay_async \
    --common.mcts.num_thread 8 --common.mcts.num_rollout_per_thread $NUM_ROLLOUTS --common.mcts.persistent_tree --common.mcts.alg_opt.c_puct 1.5 \
    --common.mcts.virtual_loss 1 --common.mcts.root_epsilon 0.25 --common.mcts.root_alpha 0.03 \
    --num_block $NUM_BLOCK --dim $DIM \
    --resign_thres 0.01 \
    --tqdm --common.net.port 2341 --q_min_size 1 --q_max_size 2000 \
    --selfplay_init_num 0 --selfplay_update_num 0 --eval_num_games 0 --gpu 0 --no_check_loaded_options --save_first \
    --client_max_delay_sec 120 "$@"
