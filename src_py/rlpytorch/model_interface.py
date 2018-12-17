# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from collections import deque

import torch
import torch.cuda
import torch.optim

from elf.options import auto_import_options, PyOptionSpec

# All model must provide .outputs and .preprocess
# E.g., .outputs = { "Q" : self.final_linear_layer }
#       .preprocess = lambda self, x: downsample(x)


class ModelInterface(object):
    """An interface for the model to receive intermediate results from
    forward passes."""

    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'opt_method',
            'optimization method (adam or sgd)',
            'adam')
        spec.addFloatOption(
            'lr',
            'learning rate',
            1e-3)
        spec.addFloatOption(
            'adam_eps',
            'Adam epsilon',
            1e-3)
        spec.addFloatOption(
            'momentum',
            'momentum parameter',
            0.9)
        spec.addFloatOption(
            'weight_decay',
            'weight decay rate',
            0.0)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization for models and optimizers.

        ``models`` is a dict that can contain multiple models in a
        single `ModelInterface`.

        For each model in ``models``, there is an optimizer in
        ``optimizers`` in correspondence, using ``torch.optim.Adam``.
        """
        self.option_map = option_map
        self.models = {}
        self.old_models = deque()
        self.optimizers = {}

    def clone(self, gpu=None):
        """Clone the state for the model interface, including
        ``models`` and ``optimizers``.

        Args:
            gpu(int): gpu id to be put the model on

        Returns:
            cloned `ModelInterface`.
        """
        mi = ModelInterface(self.option_map)
        for key, model in self.models.items():
            print ("gpu = ", gpu)
            mi.models[key] = model.clone(gpu=gpu)
            if key in self.optimizers:
                # Same parameters.
                mi.optimizers[key] = torch.optim.Adam(
                    mi.models[key].parameters())
                new_optim = mi.optimizers[key]
                old_optim = self.optimizers[key]

                new_optim_params = new_optim.param_groups[0]
                old_optim_params = old_optim.param_groups[0]
                # Copy the parameters.
                for k in new_optim_params.keys():
                    if k != "params":
                        new_optim_params[k] = old_optim_params[k]
                # Copy the state
                '''
                new_optim.state = { }
                for k, v in old_optim.state.items():
                    if isinstance(v, (int, float, str)):
                        new_optim.state[k] = v
                    else:
                        new_optim.state[k] = v.clone()
                        if gpu is not None:
                            new_optim.state[k] = new_optim.state[k].cuda(gpu)
                '''
        return mi

    def __contains__(self, key):
        return key in self.models

    def add_model(
            self,
            key,
            model,
            copy=False,
            cuda=False,
            gpu_id=None,
            opt=False,
            params={}):
        '''Add a model to `ModelInterface`.

        Args:
            key(str): key in ``self.models``.
            model(`Model`): the model to be added.
            copy(bool): indicate if the model needs to be deep copied.
            cuda(bool): indicate if model needs to be converted to cuda.
            gpu_id(int): gpu index.
            opt(bool): Whether you want your model to be optimized
                       (weights to be updated).
            params(dict): an dict of parameters for optimizers.

        Returns:
            Raise exception if key is already in ``self.models``,
            None if model is successfully added.
        '''
        if key in self.models:
            raise("ModelInterface: key[%s] is already present!" % key)

        # New model.
        if gpu_id is not None and gpu_id >= 0:
            with torch.cuda.device(gpu_id):
                self.models[key] = model.clone() if copy else model
        else:
            self.models[key] = model.clone() if copy else model
        if cuda:
            if gpu_id is not None and gpu_id >= 0:
                self.models[key].cuda(gpu_id)
            else:
                self.models[key].cuda()

        def set_default(params, ks, arg_ks=None):
            if arg_ks is None:
                arg_ks = [None] * len(ks)
            for k, arg_k in zip(ks, arg_ks):
                if arg_k is None:
                    arg_k = k
                params[k] = params.get(k, getattr(self.options, arg_k))

        curr_model = self.models[key]
        if opt or len(params) > 0:
            set_default(
                params,
                ["lr", "opt_method", "adam_eps", "momentum", "weight_decay"])

            method = params["opt_method"]

            curr_model.train()

            if method == "adam":
                self.optimizers[key] = torch.optim.Adam(
                    curr_model.parameters(), lr=params["lr"],
                    betas=(0.9, 0.999), eps=params["adam_eps"],
                    weight_decay=params["weight_decay"])
            elif method == "sgd":
                self.optimizers[key] = torch.optim.SGD(
                    curr_model.parameters(),
                    lr=params["lr"],
                    momentum=params["momentum"],
                    weight_decay=params["weight_decay"])
            else:
                raise ValueError(
                    "Optimization method %s is not supported! " %
                    params["opt_method"])

        return True

    def update_model(self, key, model, save_old_model=False):
        ''' If the key is present, update an old model. Does not deep copy it.
            If the key is not present, add it (no deep copy).

        Args:
            key(str): the key in ``models`` to be updated
            model(`Model`): updated model
        '''
        # print("Updating model " + key)
        if key not in self.models:
            self.add_model(key, model)
            return

        if save_old_model:
            self.old_models.append(self.models[key].clone().cpu())
            if len(self.old_models) > 20:
                self.old_models.popleft()

        self.models[key].load_from(model)

    def remove_model(self, key):
        del self.models[key]
        if key in self.optimizers:
            del self.optimizers[key]

    def average_model(self, key, model):
        """Average the model params from ``self.models[key]`` and ``model``,
        and update to ``self.models[key]``.

        Args:
            key(str): the key in ``models``
            model(Model): the model containing the parameters to update
        """
        for param, other_param in zip(
                self.models[key].parameters(), model.parameters()):
            param.data += other_param.data.cuda(param.data.get_device())
            param.data /= 2

    def copy(self, dst_key, src_key):
        ''' Deep copy a model from src_key to dst_key in ``self.models``

        Args:
            dst_key(str): destination key in ``self.models``
            src_key(str): source key in ``self.models``
        '''

        assert dst_key in self.models, \
            f'ModelInterface: dst_key = {dst_key} cannot be found'
        assert src_key in self.models, \
            f'ModelInterface: src_key = {src_key} cannot be found'
        self.update_model(dst_key, self.models[src_key].clone())

    ''' Usage:
        record = interface(input)
        Then record["Q"] will be the Q-function given the input.
    '''

    def zero_grad(self):
        ''' Zero the gradient for all ``optimizers`` '''
        for k, optimizer in self.optimizers.items():
            optimizer.zero_grad()

    def update_weights(self):
        """For each optimizer, call before_update for all the models,
        then update the weights and increment the step for the model."""
        for k, optimizer in self.optimizers.items():
            self.models[k].before_update()
            optimizer.step()
            self.models[k].inc_step()

    def __getitem__(self, key):
        ''' Get an item associated with ``key`` from ``self.models``'''
        return self.models[key]
