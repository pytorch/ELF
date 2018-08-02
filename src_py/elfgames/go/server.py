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

import _elf as elf
import _elfgames_go as go


class Loader(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        go.getServerPredefined(spec.getOptionSpec())

        spec.addIntOption(
            'gpu',
            'GPU id to use',
            -1)
        spec.addIntOption(
            'eval_old_model',
            ('If specified, then we directly switch to evaluation mode '
             'between the loaded model and the old model specified by this '
             'switch'),
            -1)
        spec.addStrOption(
            'comment',
            'Comment for this run',
            '')
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
        opt = go.getServerOpt(self.option_map.getOptionSpec())

        desc = {}
        GC = elf.GameContext(opt.common.base)

        mode = getattr(self.options, "common.mode")
        batchsize = getattr(self.options, "common.base.batchsize")

        if mode in ["train", "train_offline"]:
            game_obj = go.Server(opt)
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
            raise "No such mode: " + mode

        game_obj.setGameContext(GC)
        params = game_obj.getParams()

        if self.options.parameter_print:
            print("**** Options ****")
            print(opt.info())
            print("*****************")
            print("Version: ", elf.version())
            print("Mode: ", mode)
            print("Num Actions: ", params["num_action"])

        self.more_labels.add_labels(desc)
        return GCWrapper(
            GC,
            game_obj,
            batchsize,
            desc,
            num_recv=2,
            default_gpu=(self.options.gpu
                         if (self.options.gpu is not None and self.options.gpu >= 0)
                         else None),
            use_numpy=False,
            params=params,
            verbose=self.options.parameter_print)
