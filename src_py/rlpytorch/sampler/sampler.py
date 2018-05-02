# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import auto_import_options, PyOptionSpec
from .sample_methods import sample_multinomial, epsilon_greedy


class Sampler(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'sample_policy',
            'choices of epsilon-greedy, multinomial, or uniform',
            'epsilon-greedy')
        spec.addBoolOption(
            'store_greedy',
            ('if enabled, picks maximum-probability action; '
             'otherwise, sample from distribution'),
            False)
        spec.addFloatOption(
            'epsilon',
            'used in epsilon-greedy',
            0.0)
        spec.addStrListOption(
            'sample_nodes',
            'nodes to be sampled and saved',
            ['pi,a'])
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization for Sampler."""
        self.sample_nodes = []
        for nodes in self.options.sample_nodes:
            policy, action = nodes.split(",")
            self.sample_nodes.append((policy, action))

    def sample(self, state_curr):
        """Sample an action from distribution using a certain sample method

        Args:
            state_curr(dict): current state containing all data
        """
        # TODO: This only handles epsilon_greedy and multinomial for now. Add
        # uniform and original_distribution?
        sampler = (epsilon_greedy
                   if self.options.store_greedy
                   else sample_multinomial)

        actions = {}
        for pi_node, a_node in self.sample_nodes:
            actions[a_node] = sampler(state_curr, self.options, node=pi_node)
            actions[pi_node] = state_curr[pi_node].data
        return actions
