# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec
from .discounted_reward import DiscountedReward
from .utils import add_err


class Q_learning(object):
    """A Q-learning model."""

    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'a_node',
            'action node',
            'a')
        spec.addStrOption(
            'q_node',
            'Q node',
            'Q')

        spec.merge(DiscountedReward.get_option_spec())

        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization of q learning."""
        self.discounted_reward = DiscountedReward(option_map)
        self.q_loss = nn.SmoothL1Loss().cuda()

    def update(self, mi, batch, stats):
        ''' Actor critic model update.
        Feed stats for later summarization.

        Args:
            mi(`ModelInterface`): mode interface used
            batch(dict): batch of data. Keys in a batch:
                ``s``: state,
                ``r``: immediate reward,
                ``terminal``: if game is terminated
            stats(`Stats`): Feed stats for later summarization.
        '''
        m = mi["model"]
        Q_node = self.options.Q_node
        a_node = self.options.a_node

        T = batch["s"].size(0)

        state_curr = m(batch.hist(T - 1))
        Q = state_curr[Q_node].squeeze().data
        V = Q.max(1)
        self.discounted_reward.setR(V, stats)

        err = None

        for t in range(T - 2, -1, -1):
            bht = batch.hist(t)
            state_curr = m.forward(bht)

            # go through the sample and get the rewards.
            Q = state_curr[Q_node].squeeze()
            a = state_curr[a_node].squeeze()

            R = self.discounted_reward.feed(
                dict(r=batch["r"][t], terminal=batch["terminal"][t]),
                stats=stats)

            # Then you want to match Q value here.
            # Q: batchsize * #action.
            Q_sel = Q.gather(1, a.view(-1, 1)).squeeze()
            err = add_err(err, nn.L2Loss(Q_sel, Variable(R)))

        stats["cost"].feed(err.data[0] / (T - 1))
        err.backward()
