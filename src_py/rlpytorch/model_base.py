# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os
from collections import OrderedDict
from copy import deepcopy
from time import sleep

import torch
import torch.nn as nn
from torch.autograd import Variable

torch.backends.cudnn.benchmark = True


class Model(nn.Module):
    ''' Base class for an RL model, it is a wrapper for ``nn.Module``'''

    def __init__(self, option_map, params):
        """Initialize model with ``args``.

        Set ``step`` to ``0`` and ``volatile`` to ```false``.

        ``step`` records the number of times the weight has been updated.
        ``volatile`` indicates that the Variable should be used in
        inference mode, i.e. don't save the history.
        """
        super(Model, self).__init__()
        self.option_map = option_map
        self.params = params
        self.step = 0
        self.volatile = False

    def clone(self, gpu=None):
        """Deep copy an existing model.

        ``options``, ``step`` and ``state_dict`` are copied.

        Args:
            gpu(int): gpu id to be put the model on

        Returns:
            Cloned model
        """
        model = type(self)(self.option_map, self.params)
        model.load_state_dict(deepcopy(self.state_dict()))
        model.step = self.step
        if gpu is not None:
            model.cuda(gpu)
        return model

    def set_volatile(self, volatile):
        """Set model to ``volatile``.

        Args:
            volatile(bool): indicating that the Variable should be used in
                            inference mode, i.e. don't save the history.
        """
        self.volatile = volatile

    def _var(self, x):
        ''' Convert tensor x to a pytorch Variable.

        Returns:
            Variable for x
        '''
        if not isinstance(x, Variable):
            return Variable(x, volatile=self.volatile)
        else:
            return x

    def before_update(self):
        """Customized operations for each model before update.

        To be extended.

        """
        pass

    def save(self, filename, num_trial=10):
        """Save current model, step and args to ``filename``

        Args:
            filename(str): filename to be saved.
            num_trial(int): maximum number of retries to save a model.
        """
        # Avoid calling the constructor by doing self.clone()
        # deepcopy should do it
        state_dict = deepcopy(self).cpu().state_dict()

        # Note that the save might experience issues, so if we encounter
        # errors, try a few times and then give up.
        content = {
            'state_dict': state_dict,
            'step': self.step,
            'options': vars(self.options),
        }
        for i in range(num_trial):
            try:
                torch.save(content, filename)
                return
            except BaseException:
                sleep(1)
        print(
            "Failed to save %s after %d trials, giving up ..." %
            (filename, num_trial))

    def load(
            self, filename,
            omit_keys=[], replace_prefix=[], check_loaded_options=True):
        ''' Load current model, step and args from ``filename``

        Args:
            filename(str): model filename to load from
            omit_keys(list): list of omitted keys.
                             Sometimes model will have extra keys and weights
                             (e.g. due to extra tasks during training).
                             We should omit them;
                             otherwise loading will not work.
        '''
        data = torch.load(filename)

        if isinstance(data, OrderedDict):
            self.load_state_dict(data)
        else:
            for k in omit_keys:
                del data["state_dict"][k + ".weight"]
                del data["state_dict"][k + ".bias"]

            sd = data["state_dict"]

            keys = list(sd.keys())
            for key in keys:
                # Should be commented out for PyTorch > 0.40
                # if key.endswith("num_batches_tracked"):
                #    del sd[key]
                #     continue
                for src, dst in replace_prefix:
                    if key.startswith(src):
                        # print(f"Src=\"{src}\", Dst=\"{dst}\"")
                        sd[dst + key[len(src):]] = sd[key]
                        del sd[key]

            self.load_state_dict(sd)
        self.step = data.get("step", 0)
        self.filename = os.path.realpath(data.get("filename", filename))

        if check_loaded_options:
            # Ensure that for options defined in both the current model
            # options and the loaded model options, the values match between
            # current model and loaded model.
            loaded_options = data.get('options', {})
            current_options = vars(self.options)

            for option_name in \
                    (set(loaded_options.keys()) & set(current_options.keys())):
                if loaded_options[option_name] != current_options[option_name]:
                    raise ValueError(
                        f'Discrepancy between current and loaded model '
                        f'parameter: {option_name} '
                        f'loaded: {loaded_options[option_name]}, '
                        f'current: {current_options[option_name]}'
                    )

    def load_from(self, model):
        ''' Load from an existing model. State is not deep copied.
        To deep copy the model, uss ``clone``.
        '''
        if hasattr(model, 'option_map'):
            self.option_map = model.option_map

        if hasattr(model, 'params'):
            self.params = deepcopy(model.params)

        self.load_state_dict(model.state_dict())
        self.step = model.step

    def inc_step(self):
        ''' increment the step.
        ``step`` records the number of times the weight has been updated.'''
        self.step += 1

    def signature(self):
        '''Get model's signature.

        Returns:
            the model's signature string, specified by step.
        '''
        return "Model[%d]" % self.step

    def prepare_cooldown(self):
        """Prepare for "cooldown" forward passes (useful for batchnorm)."""
        pass
