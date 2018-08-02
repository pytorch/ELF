DIST_MODE=$1
shift

MODEL=$1
shift

LOAD_PATH=`hostname`
SZ=${#LOAD_PATH}

if (( "$SZ" > 15 )); then
    LOAD_PATH=${LOAD_PATH:6:3}
fi
echo $LOAD_PATH

model_file=elfgames.go.df_model3 model=df_pred game=elfgames.go.distri python -u ./experimental/df_console.py --common.mode online --distri_mode ${DIST_MODE} --common.base.num_game_thread 1 --common.base.verbose --common.mcts.verbose_time --common.mcts.num_thread 16 --common.mcts.num_rollout_per_thread -1 --following_pass --load ${MODEL} --use_mcts --common.mcts.persistent_tree --common.mcts.virtual_loss 1 --common.mcts.alg_opt.c_puct 1.5 --keys_in_reply V rv hash --replace_prefix resnet.module,resnet init_conv.module,init_conv --no_check_loaded_options --no_parameter_print --dim 224 --num_block 20 --common.base.batchsize 16 --common.mcts.num_rollout_per_batch 64 --common.net.server_id ${LOAD_PATH} --common.net.port 1978  --common.mcts.time_sec_allowed_per_move 10  "$@"
