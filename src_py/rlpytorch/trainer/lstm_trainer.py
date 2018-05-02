# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from datetime import datetime

import torch
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec
from ..stats import Stats
from ..utils import HistState
from .utils import ModelSaver, MultiCounter


class LSTMTrainer(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'freq_update',
            'frequency of model update',
            1)
        spec.addIntOption(
            'num_games',
            'number of games',
            1024)
        spec.addIntOption(
            'batchsize',
            'batch size',
            128)
        spec.addIntOption(
            'gpu',
            'which GPU to use',
            -1)
        spec.addIntOption(
            'T',
            'number of timestamps',
            6)
        spec.addStrOption(
            'parsed_args',
            'dummy option',
            '')

        spec.merge(Stats.get_option_spec('trainer'))
        spec.merge(ModelSaver.get_option_spec())

        return spec

    @auto_import_options
    def __init__(self, option_map, verbose=False):
        self.stats = Stats(option_map, "trainer")
        self.saver = ModelSaver(option_map)
        self.counter = MultiCounter()

        # [TODO] Hard coded now, need to fix.
        num_hiddens = 13 * 25

        gpu = self.options.gpu
        assert gpu is not None and gpu >= 0

        def init_state():
            return torch.FloatTensor(num_hiddens).cuda(gpu).zero_()

        self.hs = HistState(self.options.T, init_state)
        self.stats.reset()

    def episode_start(self, i):
        pass

    def actor(self, batch):
        self.counter.inc("actor")

        ids = batch["id"][0]
        seqs = batch["seq"][0]

        self.hs.preprocess(ids, seqs)
        hiddens = Variable(self.hs.newest(ids, 0))

        m = self.mi["actor"]
        m.set_volatile(True)
        state_curr = m(batch.hist(0), hiddens)
        m.set_volatile(False)

        reply_msg = self.sampler.sample(state_curr)
        reply_msg["rv"] = self.mi["actor"].step

        next_hiddens = m.transition(state_curr["h"], reply_msg["a"])

        self.hs.feed(ids, next_hiddens.data)

        self.stats.feed_batch(batch)
        return reply_msg

    def train(self, batch):
        self.counter.inc("train")
        mi = self.mi

        ids = batch["id"][0]
        T = batch["s"].size(0)

        hiddens = self.hs.newest(ids, T - 1)

        mi.zero_grad()
        self.rl_method.update(mi, batch, hiddens, self.counter.stats)
        mi.update_weights()

        if self.counter.counts["train"] % self.options.freq_update == 0:
            mi.update_model("actor", mi["model"])

    def episode_summary(self, i):
        prefix = "[%s][%d] Iter" % (
            str(datetime.now()), self.options.batchsize) + "[%d]: " % i
        print(prefix)
        if self.counter.counts["train"] > 0:
            self.saver.feed(self.mi["model"])

        print(
            "Command arguments:", ' '.join(map(str, self.options.parsed_args)))
        self.counter.summary(global_counter=i)
        print("")

        self.stats.print_summary()
        if self.stats.count_completed() > 10000:
            self.stats.reset()

    def setup(self, rl_method=None, mi=None, sampler=None):
        self.rl_method = rl_method
        self.mi = mi
        self.sampler = sampler
