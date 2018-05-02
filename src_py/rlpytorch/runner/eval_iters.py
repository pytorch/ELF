# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import auto_import_options, PyOptionSpec
from ..stats import Stats


class EvalItersBasic(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'num_eval',
            'number of games to evaluate',
            500)
        spec.addBoolOption(
            'tqdm',
            'toggle tqdm visualization',
            False)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization for Evaluation."""
        self.count = 0

    def add_count(self):
        self.count += 1

    def update_count(self, n):
        self.count = n

    def iters(self):
        ''' loop through until we reach ``num_eval`` games.

            if use ``tqdm``, also visualize the progress bar.
            Print stats summary in the end.
        '''
        if self.options.tqdm:
            import tqdm
            tq = tqdm.tqdm(total=self.options.num_eval, ncols=50)
            while self.count < self.options.num_eval:
                old_n = self.count
                yield old_n
                diff = self.count - old_n
                tq.update(diff)
            tq.close()
        else:
            while self.count < self.options.num_eval:
                yield self.count


class EvalIters(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'num_eval',
            'number of games to evaluate',
            500)
        spec.addBoolOption(
            'tqdm',
            'toggle tqdm visualization',
            False)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization for Evaluation."""
        self.stats = Stats(option_map, "eval")
        self.eval_iter_basic = EvalItersBasic(option_map)

    def _on_get_args(self, _):
        self.stats.reset()

    def iters(self):
        ''' loop through until we reach ``num_eval`` games.

            if use ``tqdm``, also visualize the progress bar.
            Print stats summary in the end.
        '''
        if self.options.tqdm:
            import tqdm
            tq = tqdm.tqdm(total=self.options.num_eval, ncols=50)
            while self.stats.count_completed() < self.options.num_eval:
                old_n = self.stats.count_completed()
                yield old_n
                diff = self.stats.count_completed() - old_n
                tq.update(diff)
            tq.close()
        else:
            while self.stats.count_completed() < self.options.num_eval:
                yield self.stats.count_completed()

        self.stats.print_summary()
