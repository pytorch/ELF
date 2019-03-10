# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec
from rlpytorch import add_err
from rlpytorch.trainer import topk_accuracy


class MultiplePrediction:
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addBoolOption(
            'multipred_backprop',
            'Whether to backprop the total loss',
            True)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.policy_loss = nn.NLLLoss().cuda()
        self.value_loss = nn.MSELoss().cuda()

    def update(self, mi, batch, stats):
        ''' Update given batch '''
        # Current timestep.
        state_curr = mi["model"](batch)
        total_policy_loss = None
        eps = 1e-6
        targets = batch["offline_a"]
        if "pis" not in state_curr:
            state_curr["pis"] = [state_curr["pi"]]

        for i, pred in enumerate(state_curr["pis"]):
            if i == 0:
                prec1, prec5 = topk_accuracy(
                    pred.data, targets[:, i].contiguous(), topk=(1, 5))
                stats["top1_acc"].feed(prec1[0])
                stats["top5_acc"].feed(prec5[0])

            # backward.
            loss = self.policy_loss(
                (pred + eps).log(), Variable(targets[:, i]))
            stats["loss" + str(i)].feed(loss.data[0])
            total_policy_loss = add_err(total_policy_loss, loss / (i + 1))

        total_value_loss = None
        if "V" in state_curr and "winner" in batch:
            total_value_loss = self.value_loss(
                state_curr["V"], Variable(batch["winner"]))

        stats["total_policy_loss"].feed(total_policy_loss.data[0])
        if total_value_loss is not None:
            stats["total_value_loss"].feed(total_value_loss.data[0])
            total_loss = total_policy_loss + total_value_loss
        else:
            total_loss = total_policy_loss

        stats["total_loss"].feed(total_loss.data[0])
        if self.options.multipred_backprop:
            total_loss.backward()
