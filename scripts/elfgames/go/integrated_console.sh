MODEL=$1
shift

model_file=elfgames.go.df_model3 model=df_pred game=elfgames.go.client python -u df_console.py --common.mode online --common.base.num_game_thread 1 --common.base.verbose --common.mcts.verbose_time --common.mcts.num_thread 16 --common.mcts.num_rollout_per_thread -1 --following_pass \
  --use_mcts --common.mcts.persistent_tree --common.mcts.virtual_loss 1 --common.mcts.alg_opt.c_puct 1.5 --keys_in_reply V rv --replace_prefix resnet.module,resnet init_conv.module,init_conv --no_check_loaded_options --no_parameter_print \
  --dim 224 --num_block 20 \
  --common.base.batchsize 16 --common.mcts.num_rollout_per_batch 16 --common.mcts.time_sec_allowed_per_move 10 "$@" \
  --load $MODEL \
