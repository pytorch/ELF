# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn

from elf.options import auto_import_options, PyOptionSpec
from rlpytorch import Model

from elfgames.go.multiple_prediction import MultiplePrediction


class Model_Policy(Model):
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
            'num_layer',
            'number of layers',
            39)
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

        # Simple method. multiple conv layers.
        self.convs = []
        self.convs_bn = []
        last_planes = self.num_planes

        for i in range(self.options.num_layer):
            conv = nn.Conv2d(last_planes, self.options.dim, 3, padding=1)
            conv_bn = (nn.BatchNorm2d(self.options.dim)
                       if self.options.bn
                       else lambda x: x)
            setattr(self, "conv" + str(i), conv)
            self.convs.append(conv)
            setattr(self, "conv_bn" + str(i), conv_bn)
            self.convs_bn.append(conv_bn)
            last_planes = self.options.dim

        self.final_conv = nn.Conv2d(
            self.options.dim, self.num_future_actions, 3, padding=1)

        # Softmax as the final layer
        self.softmax = nn.Softmax(dim=1)
        self.relu = nn.LeakyReLU(0.1) if self.options.leaky_relu else nn.ReLU()

    def forward(self, x):
        s = self._var(x["s"])

        for conv, conv_bn in zip(self.convs, self.convs_bn):
            s = conv_bn(self.relu(conv(s)))

        output = self.final_conv(s)
        pis = []
        d = self.board_size * self.board_size
        for i in range(self.num_future_actions):
            pis.append(self.softmax(output[:, i].contiguous().view(-1, d)))
        return dict(pis=pis, pi=pis[0])


# Format: key, [model, method]
Models = {
    "df_policy": [Model_Policy, MultiplePrediction]
}
