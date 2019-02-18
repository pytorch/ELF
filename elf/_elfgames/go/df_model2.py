# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn

from elf.options import auto_import_options, PyOptionSpec
from rlpytorch import Model

from elfgames.go.mcts_prediction import MCTSPrediction


class Model_PolicyValue(Model):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addBoolOption(
            'bn',
            'toggles batch norm',
            True)
        spec.addBoolOption(
            'leaky_relu',
            'toggles leaky ReLU',
            True)
        spec.addIntOption(
            'num_block',
            'number of blocks',
            20)
        spec.addIntOption(
            'dim',
            'model dimension',
            128)
        return spec

    @auto_import_options
    def __init__(self, option_map, params):
        super().__init__(option_map, params)

        self.board_size = params["board_size"]
        self.num_future_actions = params["num_future_actions"]
        self.num_planes = params["num_planes"]
        # print("#future_action: " + str(self.num_future_actions))
        # print("#num_planes: " + str(self.num_planes))

        # Network structure of AlphaGo Zero
        # https://www.nature.com/nature/journal/v550/n7676/full/nature24270.html

        # Simple method. multiple conv layers.
        self.relu = nn.LeakyReLU(0.1) if self.options.leaky_relu else nn.ReLU()
        self.convs = []
        last_planes = self.num_planes

        self.init_conv = self._conv_layer(last_planes)

        for i in range(self.options.num_block):
            conv_lower = self._conv_layer()
            conv_upper = self._conv_layer(relu=False)
            setattr(self, "conv_lower" + str(i), conv_lower)
            setattr(self, "conv_upper" + str(i), conv_upper)

            self.convs.append((conv_lower, conv_upper))

        self.pi_final_conv = self._conv_layer(self.options.dim, 2, 1)
        self.value_final_conv = self._conv_layer(self.options.dim, 1, 1)

        d = self.board_size ** 2
        self.pi_linear = nn.Linear(d * 2, d)
        self.value_linear1 = nn.Linear(d, 256)
        self.value_linear2 = nn.Linear(256, 1)

        # Softmax as the final layer
        self.logsoftmax = nn.LogSoftmax(dim=1)
        self.tanh = nn.Tanh()

    def _conv_layer(
            self,
            input_channel=None,
            output_channel=None,
            kernel=3,
            relu=True):
        if input_channel is None:
            input_channel = self.options.dim
        if output_channel is None:
            output_channel = self.options.dim

        layers = []
        layers.append(nn.Conv2d(
            input_channel,
            output_channel,
            kernel,
            padding=(kernel // 2),
        ))
        if self.options.bn:
            layers.append(nn.BatchNorm2d(output_channel))
        if relu:
            layers.append(self.relu)

        return nn.Sequential(*layers)

    def forward(self, x):
        s = self._var(x["s"])
        s = self.init_conv(s)
        for conv_lower, conv_upper in self.convs:
            s1 = conv_lower(s)
            s1 = conv_upper(s1)
            s1 = s1 + s
            s = self.relu(s1)

        d = self.board_size ** 2

        pi = self.pi_final_conv(s)
        pi = self.pi_linear(pi.view(-1, d * 2))
        logpi = self.logsoftmax(pi)
        pi = logpi.exp()

        V = self.value_final_conv(s)
        V = self.relu(self.value_linear1(V.view(-1, d)))
        V = self.value_linear2(V)
        V = self.tanh(V)

        return dict(logpi=logpi, pi=pi, V=V)


# Format: key, [model, method]
Models = {
    "df": [Model_PolicyValue, MCTSPrediction]
}
