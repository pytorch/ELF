# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import auto_import_options, PyOptionSpec


class ContextArgs(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'num_games',
            'number of games',
            1024)
        spec.addIntOption(
            'batchsize',
            'batch size',
            128)
        spec.addIntOption(
            'T',
            'number of timesteps',
            6)
        spec.addIntOption(
            'mcts_threads',
            'number of MCTS threads',
            0)
        spec.addIntOption(
            'mcts_rollout_per_batch',
            'Batch size for mcts rollout',
            1)
        spec.addIntOption(
            'mcts_rollout_per_thread',
            'number of rollotus per MCTS thread',
            1)
        spec.addBoolOption(
            'mcts_verbose',
            'enables mcts verbosity',
            False)
        spec.addBoolOption(
            'mcts_verbose_time',
            'enables mcts verbosity for time stats',
            False)
        spec.addBoolOption(
            'mcts_persistent_tree',
            'use persistent tree in MCTS',
            False)
        spec.addBoolOption(
            'mcts_use_prior',
            'use prior in MCTS',
            False)
        spec.addIntOption(
            'mcts_virtual_loss',
            '"virtual" number of losses for MCTS edges',
            0)
        spec.addStrOption(
            'mcts_pick_method',
            'criterion for mcts node selection',
            'most_visited')
        spec.addFloatOption(
            'mcts_puct',
            'prior weight',
            1.0)
        spec.addFloatOption(
            'mcts_epsilon',
            'for exploration enhancement, weight of randomization',
            0.0)
        spec.addFloatOption(
            'mcts_alpha',
            'for exploration enhancement, alpha term in gamma distribution',
            0.0)
        spec.addBoolOption(
            "mcts_unexplored_q_zero",
            'set all unexplored node to have Q value zero',
            False)
        spec.addBoolOption(
            "mcts_root_unexplored_q_zero",
            'set unexplored child of root node to have Q value zero',
            False)

        return spec

    @auto_import_options
    def __init__(self, option_map):
        pass

    def initialize(self, co):
        options = self.options
        mcts = co.mcts_options

        co.num_games = options.num_games
        co.batchsize = options.batchsize
        co.T = options.T

        mcts.num_threads = options.mcts_threads
        mcts.num_rollouts_per_thread = options.mcts_rollout_per_thread
        mcts.num_rollouts_per_batch = options.mcts_rollout_per_batch
        mcts.verbose = options.mcts_verbose
        mcts.verbose_time = options.mcts_verbose_time
        mcts.virtual_loss = options.mcts_virtual_loss
        mcts.pick_method = options.mcts_pick_method
        mcts.persistent_tree = options.mcts_persistent_tree
        mcts.root_epsilon = options.mcts_epsilon
        mcts.root_alpha = options.mcts_alpha

        mcts.alg_opt.use_prior = options.mcts_use_prior
        mcts.alg_opt.c_puct = options.mcts_puct
        mcts.alg_opt.unexplored_q_zero = options.mcts_unexplored_q_zero
        mcts.alg_opt.root_unexplored_q_zero = \
            options.mcts_root_unexplored_q_zero
