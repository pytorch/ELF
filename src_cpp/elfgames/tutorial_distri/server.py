#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os
import sys
import time

import torch
from rlpytorch import load_env, SingleProcessRun, Trainer

def main():
    print(sys.version)
    print(torch.__version__)
    print(torch.version.cuda)
    print("Conda env: \"%s\"" % os.environ.get("CONDA_DEFAULT_ENV", ""))

    additional_to_load = {
        'trainer': (
            Trainer.get_option_spec(),
            lambda option_map: Trainer(option_map)),
        'runner': (
            SingleProcessRun.get_option_spec(),
            lambda option_map: SingleProcessRun(option_map)),
    }

    env = load_env(os.environ, additional_to_load=additional_to_load)

    trainer = env['trainer']
    runner = env['runner']

    GC = env["game"].initialize()

    model_loader = env["model_loaders"][0]
    model = model_loader.load_model(GC.params)
    env["mi"].add_model("model", model, opt=True)

    GC.reg_callback("train", trainer.train)

    if GC.reg_has_callback("actor"):
        args = env["game"].options
        env["mi"].add_model(
            "actor",
            model,
            copy=True,
            cuda=(args.gpu >= 0),
            gpu_id=args.gpu)
        GC.reg_callback("actor", trainer.actor)

    trainer.setup(
        sampler=env["sampler"],
        mi=env["mi"],
        rl_method=env["method"])

    runner.setup(GC, episode_summary=trainer.episode_summary, \
            episode_start=trainer.episode_start)

    runner.run()

if __name__ == '__main__':
    main()
