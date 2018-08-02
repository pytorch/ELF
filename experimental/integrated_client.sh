GPU=$1
shift

ROOT=`python3 root_path.py`

NUM_ROLLOUTS=25
#NUM_ROLLOUTS=25

DIM=64
NUM_BLOCK=5

LOAD_PATH=`hostname`
LOAD_PATH=${LOAD_PATH:6:3}

source ../scripts/devmode_set_pythonpath.sh

root=$ROOT/$LOAD_PATH game=elfgames.go.client model=df_pred model_file=elfgames.go.df_model3 python3 -u df_selfplay.py \
    --common.mode selfplay --selfplay_timeout_usec 10 --common.base.batchsize 128 --common.base.num_game_thread 32 --keys_in_reply V rv --common.net.port 2341 --common.net.server_id $LOAD_PATH \
    --use_mcts --use_mcts_ai2 \
    --policy_distri_cutoff 30 \
    --num_block0 $NUM_BLOCK --dim0 $DIM \
    --num_block1 $NUM_BLOCK --dim1 $DIM \
    --no_check_loaded_options0 \
    --no_check_loaded_options1 \
    "$@" \
    --move_cutoff 3 \
    --cheat_eval_new_model_wins_half \
    --cheat_selfplay_random_result \
    --replace_prefix0 resnet.module,resnet init_conv.module,init_conv \
    --replace_prefix1 resnet.module,resnet init_conv.module,init_conv \
    --gpu $GPU\
