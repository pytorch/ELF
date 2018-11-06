# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn
from torch.autograd import Variable

import elf.logging as logging
from elf.options import auto_import_options, PyOptionSpec
from rlpytorch.trainer.timer import RLTimer


_logger_factory = logging.IndexedLoggerFactory(
    lambda name: logging.stderr_color_mt(name))


class MCTSPrediction(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addBoolOption(
            'backprop',
            'Whether to backprop the total loss',
            True)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.policy_loss = nn.KLDivLoss().cuda()
        self.value_loss = nn.MSELoss().cuda()
        self.logger = _logger_factory.makeLogger(
            'elfgames.go.MCTSPrediction-', '')
        self.timer = RLTimer()

    def update(self, mi, batch, stats, use_cooldown=False, cooldown_count=0):
        ''' Update given batch '''
        print("MCTSprediction.update")     # FIXME
        self.timer.restart()
        if use_cooldown:
            if cooldown_count == 0:
                mi['model'].prepare_cooldown()
                self.timer.record('prepare_cooldown')

        # Current timestep.
        state_curr = mi['model'](batch)
        self.timer.record('forward')

        if use_cooldown:
            self.logger.debug(self.timer.print(1))
            return dict(backprop=False)

        targets = batch["mcts_scores"]
        logpi = state_curr["logpi"]
        pi = state_curr["pi"]
        # backward.
        # loss = self.policy_loss(logpi, Variable(targets)) * logpi.size(1)
        loss = - (logpi * Variable(targets)
                  ).sum(dim=1).mean()  # * logpi.size(1)
        stats["loss"].feed(float(loss))
        total_policy_loss = loss

        entropy = (logpi * pi).sum() * -1 / logpi.size(0)
        stats["entropy"].feed(float(entropy))

        stats["blackwin"].feed(
            float((batch["winner"] > 0.0).float().sum()) /
            batch["winner"].size(0))

        total_value_loss = None
        if "V" in state_curr and "winner" in batch:
            total_value_loss = self.value_loss(
                state_curr["V"].squeeze(), Variable(batch["winner"]))

        stats["total_policy_loss"].feed(float(total_policy_loss))
        if total_value_loss is not None:
            stats["total_value_loss"].feed(float(total_value_loss))
            total_loss = total_policy_loss + total_value_loss
        else:
            total_loss = total_policy_loss

        stats["total_loss"].feed(float(total_loss))
        self.timer.record('feed_stats')

        if self.options.backprop:
            total_loss.backward()
            self.timer.record('backward')
            self.logger.debug(self.timer.print(1))
            return dict(backprop=True)
        else:
            self.logger.debug(self.timer.print(1))
            return dict(backprop=False)
