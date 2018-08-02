source ../scripts/devmode_set_pythonpath.sh

sh ./agz_train.sh --tqdm --common.net.port 2341 --q_min_size 1 --q_max_size 2000 --dim 64 --num_block 5 --selfplay_init_num 0 --selfplay_update_num 0 --eval_num_games 0 --gpu 0 --no_check_loaded_options --save_first --client_max_delay_sec 120 --bn_momentum 0.1 --num_cooldown 50 --selfplay_async --num_minibatch 50 "$@"
