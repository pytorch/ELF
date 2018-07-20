# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os

import torch
import torch.nn as nn
import torch.distributed as dist

from elf.options import auto_import_options, PyOptionSpec
from rlpytorch import Model

from elfgames.go.mcts_prediction import MCTSPrediction
from elfgames.go.multiple_prediction import MultiplePrediction


class Block(Model):
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
            False)
        spec.addFloatOption(
            'bn_momentum',
            'batch norm momentum (pytorch style)',
            0.1)
        spec.addFloatOption(
            "bn_eps",
            "batch norm running vars eps",
            1e-5)
        spec.addIntOption(
            'dim',
            'model dimension',
            128)
        return spec

    @auto_import_options
    def __init__(self, option_map, params):
        super().__init__(option_map, params)
        self.relu = nn.LeakyReLU(0.1) if self.options.leaky_relu else nn.ReLU()
        self.conv_lower = self._conv_layer()
        self.conv_upper = self._conv_layer(relu=False)

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
            layers.append(
                nn.BatchNorm2d(output_channel,
                               momentum=(self.options.bn_momentum or None),
                               eps=self.options.bn_eps))
        if relu:
            layers.append(self.relu)

        return nn.Sequential(*layers)

    def forward(self, s):
        s1 = self.conv_lower(s)
        s1 = self.conv_upper(s1)
        s1 = s1 + s
        s = self.relu(s1)
        return s


class GoResNet(Model):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'num_block',
            'number of resnet blocks',
            20)
        spec.merge(Block.get_option_spec())

        return spec

    @auto_import_options
    def __init__(self, option_map, params):
        super().__init__(option_map, params)
        self.blocks = []
        for _ in range(self.options.num_block):
            self.blocks.append(Block(option_map, params))
        self.resnet = nn.Sequential(*self.blocks)

    def forward(self, s):
        return self.resnet(s)


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
            False)
        spec.addFloatOption(
            'bn_momentum',
            'batch norm momentum (pytorch style)',
            0.1)
        spec.addIntOption(
            'num_block',
            'number of blocks',
            20)
        spec.addIntOption(
            'dim',
            'model dimension',
            128)
        spec.addBoolOption(
            'use_data_parallel',
            'TODO: fill this in',
            False)
        spec.addBoolOption(
            'use_data_parallel_distributed',
            'TODO: fill this in',
            False)
        spec.addIntOption(
            'dist_rank',
            'TODO: fill this in',
            -1)
        spec.addIntOption(
            'dist_world_size',
            'TODO: fill this in',
            -1)
        spec.addStrOption(
            'dist_url',
            'TODO: fill this in',
            '')
        spec.addIntOption(
            'gpu',
            'which gpu to use',
            -1)

        spec.merge(GoResNet.get_option_spec())

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
        last_planes = self.num_planes

        self.init_conv = self._conv_layer(last_planes)

        self.pi_final_conv = self._conv_layer(self.options.dim, 2, 1)
        self.value_final_conv = self._conv_layer(self.options.dim, 1, 1)

        d = self.board_size ** 2

        # Plus 1 for pass.
        self.pi_linear = nn.Linear(d * 2, d + 1)
        self.value_linear1 = nn.Linear(d, 256)
        self.value_linear2 = nn.Linear(256, 1)

        # Softmax as the final layer
        self.logsoftmax = nn.LogSoftmax(dim=1)
        self.tanh = nn.Tanh()
        self.resnet = GoResNet(option_map, params)

        if torch.cuda.is_available() and self.options.gpu is not None:
            self.init_conv.cuda(self.options.gpu)
            self.resnet.cuda(self.options.gpu)

        if self.options.use_data_parallel:
            if self.options.gpu is not None:
                self.init_conv = nn.DataParallel(
                    self.init_conv, output_device=self.options.gpu)
                self.resnet = nn.DataParallel(
                    self.resnet, output_device=self.options.gpu)

        self._check_and_init_distributed_model()

    def _check_and_init_distributed_model(self):
        if not self.options.use_data_parallel_distributed:
            return

        if not dist.is_initialized():
            world_size = self.options.dist_world_size
            url = self.options.dist_url
            rank = self.options.dist_rank
            # This is for SLURM's special use case
            if rank == -1:
                rank = int(os.environ.get("SLURM_NODEID"))

            print("=> Distributed training: world size: {}, rank: {}, URL: {}".
                  format(world_size, rank, url))

            dist.init_process_group(backend="nccl",
                                    init_method=url,
                                    rank=rank,
                                    world_size=world_size)

        # Initialize the distributed data parallel model
        master_gpu = self.options.gpu
        if master_gpu is None or master_gpu < 0:
            raise RuntimeError("Distributed training requires "
                               "to put the model on the GPU, but the GPU is "
                               "not given in the argument")
        # This is needed for distributed model since the distributed model
        # initialization will require the model be on the GPU, even though
        # the later code will put the same model on the GPU again with
        # self.options.gpu, so this should be ok
        # self.resnet.cuda(master_gpu)
        self.init_conv = nn.parallel.DistributedDataParallel(
            self.init_conv)
        self.resnet = nn.parallel.DistributedDataParallel(
            self.resnet)

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
            padding=(kernel // 2)
        ))
        if self.options.bn:
            layers.append(
                nn.BatchNorm2d(output_channel,
                               momentum=(self.options.bn_momentum or None),
                               eps=self.options.bn_eps))
        if relu:
            layers.append(self.relu)

        return nn.Sequential(*layers)

    def prepare_cooldown(self):
        try:
            for module in self.modules():
                if module.__class__.__name__.startswith('BatchNorm'):
                    module.reset_running_stats()
        except Exception as e:
            print(e)
            print("The module doesn't have method 'reset_running_stats', "
                  "skipping. Please set bn_momentum to 0.1"
                  "(for cooldown = 50) in this case")

    def forward(self, x):
        s = self._var(x["s"])

        s = self.init_conv(s)
        s = self.resnet(s)

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
    "df_pred": [Model_PolicyValue, MultiplePrediction],
    "df_kl": [Model_PolicyValue, MCTSPrediction]
}
