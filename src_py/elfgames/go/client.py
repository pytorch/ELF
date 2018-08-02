#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os

from elf import GCWrapper, MoreLabels
from elf.options import auto_import_options, PyOptionSpec

import _elf as elf
import _elfgames_go as go


class Loader(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        go.getClientPredefined(spec.getOptionSpec())

        spec.addIntOption(
            'gpu',
            'GPU id to use',
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

        spec.addIntOption(
            'selfplay_timeout_usec',
            'Timeout used for MCTS',
            10)

        spec.addBoolOption(
            "parameter_print",
            "Print parameters",
            True)

        spec.merge(PyOptionSpec.fromClasses((MoreLabels,)))
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.more_labels = MoreLabels(option_map)
        self.option_map = option_map

    def initialize(self):
        job_id = os.environ.get("job_id", "local")
        opt = go.getClientOpt(self.option_map.getOptionSpec(), job_id)
        print("2")
        mode = getattr(self.options, "common.mode")
        batchsize = getattr(self.options, "common.base.batchsize")

        GC = elf.GameContext(opt.common.base)

        if mode not in ["online", "selfplay"]:
            raise "No such mode: " + mode
        print("3")
        game_obj = go.Client(opt)
        game_obj.setGameContext(GC)

        params = game_obj.getParams()
        print("4")
        if self.options.parameter_print:
            print("**** Options ****")
            print(opt.info())
            print("*****************")
            print("Version: ", elf.version())
            print("Mode: ", mode)
            print("Num Actions: ", params["num_action"])

        desc = {}
        if mode == "online":
            desc["human_actor"] = dict(
                input=[],
                reply=["a", "timestamp", "timeleft", "byoyomi"],
                batchsize=1,
            )
            # Used for MCTS/Direct play.
            desc["actor_black"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                timeout_usec=10,
                batchsize=getattr(
                    self.options, "common.mcts.num_rollout_per_batch")
            )
        elif mode == "selfplay":
            # Used for MCTS/Direct play.
            white_batchsize = self.options.white_mcts_rollout_per_batch
            if white_batchsize < 0:
                white_batchsize = batchsize

            desc["actor_black"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                batchsize=batchsize,
                timeout_usec=self.options.selfplay_timeout_usec,
            )

            desc["actor_white"] = dict(
                input=["s"],
                reply=["pi", "V", "a", "rv"],
                batchsize=white_batchsize,
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

        self.more_labels.add_labels(desc)
        return GCWrapper(
            GC,
            game_obj,
            batchsize,
            desc,
            num_recv=8,
            default_gpu=(self.options.gpu
                         if (self.options.gpu is not None and self.options.gpu >= 0)
                         else None),
            use_numpy=False,
            params=params,
            verbose=self.options.parameter_print)
