#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os

from elf import GCWrapper, ContextArgs, MoreLabels
from elf.options import auto_import_options, PyOptionSpec

import _elfgames_tasks as go
from server_addrs import addrs


class Loader(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'preload_sgf',
            'TODO: fill this help message in',
            '')
        spec.addIntOption(
            'preload_sgf_move_to',
            'TODO: fill this help message in',
            -1)
        spec.addBoolOption(
            'actor_only',
            'TODO: fill this help message in',
            False)
        spec.addStrListOption(
            'list_files',
            'Provide a list of json files for offline training',
            [])
        spec.addIntOption(
            'port',
            'TODO: fill this help message in',
            5556)
        spec.addStrOption(
            'server_addr',
            'TODO: fill this help message in',
            '')
        spec.addStrOption(
            'server_id',
            'TODO: fill this help message in',
            '')
        spec.addIntOption(
            'q_min_size',
            'TODO: fill this help message in',
            10)
        spec.addIntOption(
            'q_max_size',
            'TODO: fill this help message in',
            1000)
        spec.addIntOption(
            'num_reader',
            'TODO: fill this help message in',
            50)
        spec.addIntOption(
            'num_reset_ranking',
            'TODO: fill this help message in',
            5000)
        spec.addIntOption(
            'client_max_delay_sec',
            'Maximum amount of allowed delays in sec. If the client '
            'didn\'t respond after that, we think it is dead.',
            1200)
        spec.addBoolOption(
            'verbose',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'keep_prev_selfplay',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'print_result',
            'TODO: fill this help message in',
            False)
        spec.addIntOption(
            'data_aug',
            'specify data augumentation, 0-7, -1 mean random',
            -1)
        spec.addIntOption(
            'ratio_pre_moves',
            ('how many moves to perform in each thread, before we use the '
             'data to train the model'),
            0)
        spec.addFloatOption(
            'start_ratio_pre_moves',
            ('how many moves to perform in each thread, before we use the '
             'first sgf file to train the model'),
            0.5)
        spec.addIntOption(
            'num_games_per_thread',
            ('For offline mode, it is the number of concurrent games per '
             'thread, used to increase diversity of games; for selfplay mode, '
             'it is the number of games played at each thread, and after that '
             'we need to call restartAllGames() to resume.'),
            -1)
        spec.addIntOption(
            'expected_num_clients',
            'Expected number of clients',
            -1
        )
        spec.addIntOption(
            'num_future_actions',
            'TODO: fill this help message in',
            1)
        spec.addIntOption(
            'move_cutoff',
            'Cutoff ply in replay',
            -1)
        spec.addStrOption(
            'mode',
            'TODO: fill this help message in',
            'online')
        spec.addBoolOption(
            'black_use_policy_network_only',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'white_use_dga',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'black_use_dga',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'white_use_policy_network_only',
            'TODO: fill this help message in',
            False)
        spec.addIntOption(
            'ply_pass_enabled',
            'TODO: fill this help message in',
            0)
        spec.addBoolOption(
            'use_mcts',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'use_mcts_ai2',
            'TODO: fill this help message in',
            False)
        spec.addFloatOption(
            'white_puct',
            'PUCT for white when it is > 0.0. If it is -1 then we use'
            'the same puct for both side (specified by mcts_options).'
            'A HACK to use different puct for different model. Should'
            'be replaced by a more systematic approach.',
            -1.0)
        spec.addIntOption(
            'white_mcts_rollout_per_batch',
            'white mcts rollout per batch',
            -1)
        spec.addIntOption(
            'white_mcts_rollout_per_thread',
            'white mcts rollout per thread',
            -1)
        spec.addBoolOption(
            'use_df_feature',
            'TODO: fill this help message in',
            False)
        spec.addStrOption(
            'dump_record_prefix',
            'TODO: fill this help message in',
            '')
        spec.addIntOption(
            'policy_distri_cutoff',
            'TODO: fill this help message in',
            0)
        spec.addFloatOption(
            'resign_thres',
            'TODO: fill this help message in',
            0.0)
        spec.addBoolOption(
            'following_pass',
            'TODO: fill this help message in',
            False)
        spec.addIntOption(
            'selfplay_timeout_usec',
            'TODO: fill this help message in',
            0)
        spec.addIntOption(
            'gpu',
            'TODO: fill this help message in',
            -1)
        spec.addBoolOption(
            'policy_distri_training_for_all',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'parameter_print',
            'TODO: fill this help message in',
            True)
        spec.addIntOption(
            'batchsize',
            'batch size',
            128)
        spec.addIntOption(
            'batchsize2',
            'batch size',
            -1)
        spec.addIntOption(
            'T',
            'number of timesteps',
            6)
        spec.addIntOption(
            'selfplay_init_num',
            ('Initial number of selfplay games to generate before training a '
             'new model'),
            2000)
        spec.addIntOption(
            'selfplay_update_num',
            ('Additional number of selfplay games to generate after a model '
             'is updated'),
            1000)
        spec.addBoolOption(
            'selfplay_async',
            ('Whether to use async mode in selfplay'),
            False)
        spec.addIntOption(
            'eval_num_games',
            ('number of evaluation to be performed to decide whether a model '
             'is better than the other'),
            400)
        spec.addFloatOption(
            'eval_winrate_thres',
            'Win rate threshold for evalution',
            0.55)
        spec.addIntOption(
            'eval_old_model',
            ('If specified, then we directly switch to evaluation mode '
             'between the loaded model and the old model specified by this '
             'switch'),
            -1)
        spec.addStrOption(
            'eval_model_pair',
            ('If specified for df_selfplay.py, then the two models will be '
             'evaluated on this client'),
            '')
        spec.addStrOption(
            'comment',
            'Comment for this run',
            '')
        spec.addBoolOption(
            'cheat_eval_new_model_wins_half',
            'When enabled, in evaluation mode, when the game '
            'finishes, the player with the most recent model gets 100% '
            'win rate half of the time.'
            'This is used to test the framework',
            False)
        spec.addBoolOption(
            'cheat_selfplay_random_result',
            'When enabled, in selfplay mode the result of the game is random'
            'This is used to test the framework',
            False)
        spec.addIntOption(
            'suicide_after_n_games',
            'return after n games have finished, -1 means it never ends',
            -1)

        spec.merge(PyOptionSpec.fromClasses((ContextArgs, MoreLabels)))

        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.context_args = ContextArgs(option_map)
        self.more_labels = MoreLabels(option_map)

    def _set_params(self):
        co = go.ContextOptions()
        self.context_args.initialize(co)
        co.job_id = os.environ.get("job_id", "local")
        if self.options.parameter_print:
            co.print()

        opt = go.GameOptions()
        opt.seed = 0
        opt.list_files = self.options.list_files

        if self.options.server_addr:
            opt.server_addr = self.options.server_addr
        else:
            if self.options.server_id and self.options.server_id != "none":
                opt.server_addr = addrs[self.options.server_id]
                opt.server_id = self.options.server_id
            else:
                opt.server_addr = ""
                opt.server_id = ""

        opt.port = self.options.port
        opt.mode = self.options.mode
        opt.use_mcts = self.options.use_mcts
        opt.use_mcts_ai2 = self.options.use_mcts_ai2
        opt.use_df_feature = self.options.use_df_feature
        opt.dump_record_prefix = self.options.dump_record_prefix
        opt.policy_distri_training_for_all = \
            self.options.policy_distri_training_for_all
        opt.verbose = self.options.verbose
        opt.black_use_policy_network_only = \
            self.options.black_use_policy_network_only
        opt.white_use_policy_network_only = \
            self.options.white_use_policy_network_only
        opt.black_use_dga = \
            self.options.black_use_dga
        opt.white_use_dga = \
            self.options.white_use_dga
        opt.data_aug = self.options.data_aug
        opt.ratio_pre_moves = self.options.ratio_pre_moves
        opt.q_min_size = self.options.q_min_size
        opt.q_max_size = self.options.q_max_size
        opt.num_reader = self.options.num_reader
        opt.start_ratio_pre_moves = self.options.start_ratio_pre_moves
        opt.ply_pass_enabled = self.options.ply_pass_enabled
        opt.num_future_actions = self.options.num_future_actions
        opt.num_reset_ranking = self.options.num_reset_ranking
        opt.move_cutoff = self.options.move_cutoff
        opt.policy_distri_cutoff = self.options.policy_distri_cutoff
        opt.num_games_per_thread = self.options.num_games_per_thread
        opt.following_pass = self.options.following_pass
        opt.resign_thres = self.options.resign_thres
        opt.preload_sgf = self.options.preload_sgf
        opt.preload_sgf_move_to = self.options.preload_sgf_move_to
        opt.keep_prev_selfplay = self.options.keep_prev_selfplay
        opt.expected_num_clients = self.options.expected_num_clients

        opt.white_puct = self.options.white_puct
        opt.white_mcts_rollout_per_batch = \
            self.options.white_mcts_rollout_per_batch
        opt.white_mcts_rollout_per_thread = \
            self.options.white_mcts_rollout_per_thread

        opt.client_max_delay_sec = self.options.client_max_delay_sec
        opt.print_result = self.options.print_result
        opt.selfplay_init_num = self.options.selfplay_init_num
        opt.selfplay_update_num = self.options.selfplay_update_num
        opt.selfplay_async = self.options.selfplay_async
        opt.eval_num_games = self.options.eval_num_games
        opt.eval_thres = self.options.eval_winrate_thres
        opt.cheat_eval_new_model_wins_half = \
            self.options.cheat_eval_new_model_wins_half
        opt.cheat_selfplay_random_result = \
            self.options.cheat_selfplay_random_result

        self.max_batchsize = max(
            self.options.batchsize, self.options.batchsize2) \
            if self.options.batchsize2 > 0 \
            else self.options.batchsize
        co.batchsize = self.max_batchsize

        GC = go.GameContext(co, opt)

        if self.options.parameter_print:
            print("**** Options ****")
            print(opt.info())
            print("*****************")
            print("Version: ", GC.ctx().version())

        return co, GC, opt

    def initialize(self):
        co, GC, opt = self._set_params()

        params = GC.getParams()

        if self.options.parameter_print:
            print("Mode: ", opt.mode)
            print("Num Actions: ", params["num_action"])

        desc = {}
        if self.options.mode == "online":
            desc["human_actor"] = dict(
                input=["s"],
                reply=["pi", "a", "V"],
                batchsize=1,
            )
            # Used for MCTS/Direct play.
            desc["actor_black"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                timeout_usec=10,
                batchsize=co.mcts_options.num_rollouts_per_batch
            )
        elif self.options.mode == "selfplay":
            # Used for MCTS/Direct play.
            desc["actor_black"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                batchsize=self.options.batchsize,
                timeout_usec=self.options.selfplay_timeout_usec,
            )
            desc["actor_white"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                batchsize=self.options.batchsize2
                if self.options.batchsize2 > 0
                else self.options.batchsize,
                timeout_usec=self.options.selfplay_timeout_usec,
            )
            desc["game_end"] = dict(
                batchsize=1,
            )
            desc["game_start"] = dict(
                batchsize=1,
                input=["black_ver", "white_ver"],
                reply=None
            )
        elif self.options.mode == "train" or \
                self.options.mode == "offline_train":
            desc["train"] = dict(
                input=["s", "offline_a", "winner", "mcts_scores", "move_idx",
                       "selfplay_ver"],
                reply=None
            )
            desc["train_ctrl"] = dict(
                input=["selfplay_ver"],
                reply=None,
                batchsize=1
            )
        else:
            raise "No such mode: " + self.options.mode

        params.update(dict(
            num_group=1 if self.options.actor_only else 2,
            T=self.options.T,
        ))

        self.more_labels.add_labels(desc)
        return GCWrapper(
            GC,
            self.max_batchsize,
            desc,
            num_recv=2,
            gpu=(self.options.gpu
                 if (self.options.gpu is not None and self.options.gpu >= 0)
                 else None),
            use_numpy=False,
            params=params,
            verbose=self.options.parameter_print)
