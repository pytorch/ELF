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

import _elfgames_go_inference as go
# from server_addrs import addrs


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
        spec.addStrOption(
            'mode',
            'TODO: fill this help message in',
            "online")
        spec.addBoolOption(
            'actor_only',
            'TODO: fill this help message in',
            False)
        spec.addIntOption(
            'num_reset_ranking',
            'TODO: fill this help message in',
            5000)
        spec.addBoolOption(
            'verbose',
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
            'num_games_per_thread',
            ('For offline mode, it is the number of concurrent games per '
             'thread, used to increase diversity of games; for selfplay mode, '
             'it is the number of games played at each thread, and after that '
             'we need to call restartAllGames() to resume.'),
            -1)
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
        spec.addIntOption(
            'ply_pass_enabled',
            'TODO: fill this help message in',
            0)
        spec.addBoolOption(
            'use_mcts',
            'TODO: fill this help message in',
            False)
        spec.addBoolOption(
            'use_df_feature',
            'TODO: fill this help message in',
            False)
        spec.addStrOption(
            'dump_record_prefix',
            'TODO: fill this help message in',
            '')
        spec.addFloatOption(
            'resign_thres',
            'TODO: fill this help message in',
            0.0)
        spec.addBoolOption(
            'following_pass',
            'TODO: fill this help message in',
            False)
        spec.addIntOption(
            'gpu',
            'TODO: fill this help message in',
            -1)
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
        spec.addFloatOption(
            'eval_winrate_thres',
            'Win rate threshold for evalution',
            0.55)
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

        opt.mode = self.options.mode
        opt.use_mcts = self.options.use_mcts
        opt.use_df_feature = self.options.use_df_feature
        opt.dump_record_prefix = self.options.dump_record_prefix
        opt.verbose = self.options.verbose
        opt.black_use_policy_network_only = \
            self.options.black_use_policy_network_only
        opt.data_aug = self.options.data_aug
        opt.ply_pass_enabled = self.options.ply_pass_enabled
        opt.num_reset_ranking = self.options.num_reset_ranking
        opt.move_cutoff = self.options.move_cutoff
        opt.num_games_per_thread = self.options.num_games_per_thread
        opt.following_pass = self.options.following_pass
        opt.resign_thres = self.options.resign_thres
        opt.preload_sgf = self.options.preload_sgf
        opt.preload_sgf_move_to = self.options.preload_sgf_move_to

        opt.print_result = self.options.print_result

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
