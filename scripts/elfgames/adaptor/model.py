# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os

import torch
import torch.nn as nn

from elf.options import auto_import_options, PyOptionSpec
from rlpytorch import Model
from elfgames.tutorial_distri.optim_method import MyOptim

class MyModel(Model):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'gpu',
            'which gpu to use',
            -1)

        return spec

    @auto_import_options
    def __init__(self, option_map, params):
        super().__init__(option_map, params)

        self.relu = nn.ReLU()

        input_dim = params["input_dim"]
        num_action = params["num_action"]

        latent_dim = 200

        self.trunk = nn.Linear(input_dim, latent_dim)
        self.pi_linear = nn.Linear(latent_dim, num_action)
        self.value_linear = nn.Linear(latent_dim, 1)

        # Softmax as the final layer
        self.logsoftmax = nn.LogSoftmax(dim=1)
        self.tanh = nn.Tanh()

        self.input_dim = input_dim
        self.latent_dim = latent_dim

    def forward(self, x):
        s = self._var(x["s"])

        s = self.trunk(s.view(-1, self.input_dim))
        s = self.relu(s)
        
        pi = self.pi_linear(s)
        logpi = self.logsoftmax(pi)
        pi = logpi.exp()

        V = self.value_linear(s)
        V = self.tanh(V)

        return dict(logpi=logpi, pi=pi, V=V)


# Format: key, [model, method]
Models = {
    "simple": [MyModel, MyOptim],
}
