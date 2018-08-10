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

from datetime import datetime

from rlpytorch import \
    Evaluator, load_env, ModelInterface

import torch

def main():
    print('Python version:', sys.version)
    print('PyTorch version:', torch.__version__)
    print('CUDA version', torch.version.cuda)
    print('Conda env:', os.environ.get("CONDA_DEFAULT_ENV", ""))

    # Set game to online model.
    actors = ["actor"]
    additional_to_load = {
        ("eval_" + actor_name): (
            Evaluator.get_option_spec(name="eval_" + actor_name),
            lambda object_map, actor_name=actor_name: Evaluator(
                object_map, name="eval_" + actor_name,
                actor_name=actor_name, stats=None)
        )
        for i, actor_name in enumerate(actors)
    }
    additional_to_load.update({
        ("mi_" + name): (ModelInterface.get_option_spec(), ModelInterface)
        for name in actors
    })

    env = load_env(
        os.environ, num_models=1,
        additional_to_load=additional_to_load)

    GC = env["game"].initialize()
    args = env["game"].options
    model = env["model_loaders"][0].load_model(GC.params)

    # for actor_name, stat, model_loader, e in \
    #         zip(actors, stats, env["model_loaders"], evaluators):
    for i in range(len(actors)):
        actor_name = actors[i]
        e = env["eval_" + actor_name]
        mi = env["mi_" + actor_name]

        mi.add_model("actor", model, cuda=(args.gpu >= 0), gpu_id=args.gpu)

        print("register " + actor_name + " for e = " + str(e))
        e.setup(sampler=env["sampler"], mi=mi)

        def actor(batch, e):
            reply = e.actor(batch)
            return reply

        GC.reg_callback(actor_name, lambda batch, e=e: actor(batch, e))

    args = env["game"].options

    GC.start()
    for actor_name in actors:
        env["eval_" + actor_name].episode_start(0)

    while True:
        GC.run()

    GC.stop()

if __name__ == '__main__':
    main()

