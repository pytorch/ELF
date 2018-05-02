# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec
from .utils import add_err
from .discounted_reward import DiscountedReward
from .policy_gradient import PolicyGradient
from .value_matcher import ValueMatcher


class RNNActorCritic(object):
    """RNN actor-critic model."""

    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'value_node',
            'name of the value node',
            'V')

        spec.merge(PyOptionSpec.fromClasses(
            (PolicyGradient, DiscountedReward, ValueMatcher)
        ))

        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization of RNNActorCritic component methods:

        `PolicyGradient`, `DiscountedReward`, and `ValueMatcher`.
        """
        self.discounted_reward = DiscountedReward()
        self.pg = PolicyGradient()
        self.value_matcher = ValueMatcher()

    def update(self, mi, batch, hiddens, stats):
        m = mi["model"]
        value_node = self.options.value_node

        T = batch["a"].size(0)

        h = Variable(hiddens)
        hs = []
        ss = []

        # Forward to compute LSTM.
        for t in range(0, T - 1):
            if t > 0:
                term = Variable(1.0 - batch["terminal"][t].float()).view(-1, 1)
                h.register_hook(lambda grad: grad.mul(term))

            state_curr = m(batch.hist(t), h)
            h = m.transition(state_curr["h"], batch["a"][t])
            hs.append(h)
            ss.append(state_curr)

        R = ss[-1][value_node].squeeze().data
        self.discounted_reward.setR(R, stats)

        err = None

        # Backward to compute gradient descent.
        for t in range(T - 2, -1, -1):
            state_curr = ss[t]

            # go through the sample and get the rewards.
            bht = batch.hist(t)
            V = state_curr[value_node].squeeze()

            R = self.discounted_reward.feed(
                dict(r=batch["r"][t], terminal=batch["terminal"][t]),
                stats)

            err = add_err(
                err,
                self.pg.feed(
                    R - V.data,
                    state_curr,
                    bht,
                    stats,
                    old_pi_s=bht))
            err = add_err(err, self.value_matcher.feed(
                {value_node: V, "target": R}, stats))

        stats["cost"].feed(err.data[0] / (T - 1))
        err.backward()
