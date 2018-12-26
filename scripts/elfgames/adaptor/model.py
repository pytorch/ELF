# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os

import torch
import torch.nn as nn
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec

class MyModel(nn.Module):
    def __init__(self, params):
        super().__init__()

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
        s = Variable(x["s"])

        s = self.trunk(s.view(-1, self.input_dim))
        s = self.relu(s)

        pi = self.pi_linear(s)
        logpi = self.logsoftmax(pi)
        pi = logpi.exp()

        V = self.value_linear(s)
        V = self.tanh(V)

        return dict(logpi=logpi, pi=pi, V=V)

