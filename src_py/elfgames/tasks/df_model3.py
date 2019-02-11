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

OUTSIZE = 64 * 3

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
#            1)

        spec.merge(GoResNet.get_option_spec())

        return spec

    @auto_import_options
    def __init__(self, option_map, params):
        super().__init__(option_map, params)

        self.board_size = None  #params["board_size"]
        self.num_future_actions = OUTSIZE #params["num_future_actions"]
        self.num_planes = params["num_planes"]
        #print("my board size is " + str(self.board_size))
        #print("my num future actions is " + str(self.num_future_actions))
        #print("num_planes = " + str(self.num_planes))
        # print("#future_action: " + str(self.num_future_actions))
        # print("#num_planes: " + str(self.num_planes))

        # Network structure of AlphaGo Zero
        # https://www.nature.com/nature/journal/v550/n7676/full/nature24270.html

        # Simple method. multiple conv layers.
        self.relu = nn.LeakyReLU(0.1) if self.options.leaky_relu else nn.ReLU()
        last_planes = self.num_planes
        #print("number of planes = " + str(last_planes))
        self.init_conv = self._conv_layer(last_planes)
        #print("self.options.dim=", self.options.dim)

        self.pi_final_conv = self._conv_layer(2,2,1) #self.options.dim, 2, 8)#self.num_planes)
        self.value_final_conv = self._conv_layer(self.options.dim, 1, 1)

        d = OUTSIZE # FIXME self.board_size ** 2

        # Plus 1 for pass.
        # 128 below is the total size h x v x #channels (#channels is dim is the bash script)
        self.pi_linear = nn.Linear(128,192)#OUTSIZE/3)#OUTSIZE)   #d * 3, d + 1)    # FIXME: at most 256 actions...
        self.value_linear1 = nn.Linear(64, 256)  #(d, 256)
        self.value_linear2 = nn.Linear(256, 1)

#        self.pi_final_conv = self._conv_layer(self.options.dim, OUTSIZE, 1)#256, OUTSIZE)
#        self.value_final_conv = self._conv_layer(self.options.dim, 1, 1)
#
#        d = OUTSIZE # FIXME self.board_size ** 2
#
#        # Plus 1 for pass.
#        self.pi_linear = nn.Linear(OUTSIZE, d) #d * 2, d + 1)    # FIXME: at most 256 actions...
#        self.value_linear1 = nn.Linear(1,256)  #(d, 256)
#        self.value_linear2 = nn.Linear(256, 1)

        # Softmax as the final layer
        self.logsoftmax = nn.LogSoftmax(dim=1)
        self.tanh = nn.Tanh()
        self.resnet = GoResNet(option_map, params)

        if torch.cuda.is_available() and self.options.gpu is not None:
            self.init_conv.cuda(self.options.gpu)
            self.resnet.cuda(self.options.gpu)
            #self.init_conv.cuda(1)
            #self.resnet.cuda(1)

        if self.options.use_data_parallel:
            if self.options.gpu is not None:
                self.init_conv = nn.DataParallel(
                    self.init_conv, output_device=self.options.gpu)
                self.resnet = nn.DataParallel(
                    self.resnet, output_device=self.options.gpu)

        self._check_and_init_distributed_model()
        #print("df_model3 initialization ok")

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

            #print("=> Distributed training: world size: {}, rank: {}, URL: {}".format(world_size, rank, url))

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
        assert input_channel < 300, str(input_channel)
        #print(str([input_channel, output_channel, kernel, relu]))
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

    def old_forward(self, x):
        d = 256  # This is problem dependent ! FIXME
        #print("farwardgo")
        s = self._var(x["s"])
        #print("farward0")

        pi = nn.Linear(1,d).cuda(self.options.gpu)(s.view(-1, 1 ))
        #print("farward2")
        logpi = self.logsoftmax(pi)
        #print("farward3")
        pi = logpi.exp()
        #print("farward4")
        V = nn.Linear(1,d)(s.view(-1,d))
        #print("farward5")
        V = self.tanh(V)
        #print("farward8")

        return dict(logpi=logpi, pi=pi, V=V)





    def forward(self, x):
        #print("forward1")
        s = self._var(x["s"])
        print("forward2   initial s = ", s.size(), "      should be batchsize x channels x h x v")


        s = self.init_conv(s)
        print("forward3", s.size(), " this s should be  batchsize x channels' x h x v")
        s = self.resnet(s)
        print("forward4", s.size(), " should be batchsize x channels' x h x v")

        d = None

        pi = self.pi_final_conv(s)
        print("forward6 " + str(pi.size()) + " should be of size batchsize x 2 x h x v")
        pi = pi.view(-1, 128)  # this should match the input size in pi_linear
        print("forward6.5 " + str(pi.size()) + " should be of size xxx, is ")
        pi = self.pi_linear(pi)    # This 256 is weird... we just take care of misalign, otherwise it's 2. FIXME
        print("forward7 " + str(pi.size()) + " should be of size batchsize x num_actions")
        logpi = self.logsoftmax(pi)
        print("forward8 " + str(logpi.size()))
        pi = logpi.exp()
        print("forward9" + str(pi.size()))

        V = self.value_final_conv(s)
        print("forward10", V.size(), " should be batchsize x h*v")
        V2 = V.view(-1, 64)
        V3 = self.value_linear1(V2)
        V = self.relu(V3) 
        print(V.size(), " should be batchsize x 256")
        #V = self.relu(self.value_linear1(V.view(-1, d)))
        #print("forward11")
        print("")
        V = self.value_linear2(V)
        print("forward12", V.size(), " instead of batchsize x 1")
        V = self.tanh(V)
        #print("forward13")

        return dict(logpi=logpi, pi=pi, V=V)


# Format: key, [model, method]
Models = {
    "df_pred": [Model_PolicyValue, MultiplePrediction],
    "df_kl": [Model_PolicyValue, MCTSPrediction]
}
