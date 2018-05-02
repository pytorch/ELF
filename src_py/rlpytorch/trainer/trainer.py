# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os
import sys
from datetime import datetime

from elf.options import auto_import_options, import_options, PyOptionSpec
from ..stats import Stats
from .timer import RLTimer
from .utils import ModelSaver, MultiCounter

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'elf'))

# import torch.multiprocessing as _mp
# mp = _mp.get_context('spawn')


class Evaluator(object):
    @classmethod
    def get_option_spec(cls, name='eval'):
        spec = PyOptionSpec()
        spec.addStrListOption(
            'keys_in_reply',
            'keys in reply',
            [])
        spec.addIntOption(
            'num_minibatch',
            'number of minibatches',
            5000)
        spec.addStrListOption(
            'parsed_args',
            'dummy option',
            '')

        spec.merge(Stats.get_option_spec(name))

        return spec

    def __init__(
            self,
            option_map,
            name='eval',
            stats=True,
            verbose=False,
            actor_name="actor"):
        """Initialization for Evaluator."""
        import_options(self, option_map, self.get_option_spec(name))

        if stats:
            self.stats = Stats(option_map, name)
        else:
            self.stats = None
        if self.stats is not None and not self.stats.is_valid():
            self.stats = None

        self.name = name
        self.actor_name = actor_name
        self.verbose = verbose
        self.keys_in_reply = set(self.options.keys_in_reply)

    def episode_start(self, i):
        ''' Called before each episode. Reset ``actor_count`` to 0.

        Args:
            i(int): index in the minibatch
        '''
        self.actor_count = 0

    def actor(self, batch):
        """Actor.

        Get the model, forward the batch and get a distribution.

        Sample from it and act.

        Reply the message to game engine.

        Args:
            batch(dict): batch data

        Returns:
            reply_msg(dict):
                ``pi``: policy, ``a``: action,
                ``V``: value, `rv`: reply version,
                signatured by step
        """
        if self.verbose:
            print("In Evaluator[%s]::actor" % self.name)

        # actor model.
        m = self.mi[self.actor_name]
        m.set_volatile(True)
        state_curr = m.forward(batch)
        m.set_volatile(False)

        if self.sampler is not None:
            reply_msg = self.sampler.sample(state_curr)
        else:
            reply_msg = dict(pi=state_curr["pi"].data)

        if self.stats is not None:
            self.stats.feed_batch(batch)

        if "rv" in self.keys_in_reply:
            reply_msg["rv"] = self.mi[self.actor_name].step

        if "V" in self.keys_in_reply:
            reply_msg["V"] = state_curr["V"].data

        self.actor_count += 1
        return reply_msg

    def episode_summary(self, i):
        ''' Called after each episode. Print stats and summary

        Args:
            i(int): index in the minibatch
        '''
        print(
            "[%s] actor count: %d/%d" %
            (self.name,
             self.actor_count,
             self.options.num_minibatch))

        if self.stats is not None:
            self.stats.print_summary()
            if self.stats.count_completed() > 10000:
                self.stats.reset()

    def setup(self, mi=None, sampler=None):
        ''' Setup `ModelInterface` and `Sampler`. Resetting stats.

        Args:
            mi(`ModelInterface`)
            sample(`Sampler`)
        '''
        self.mi = mi
        self.sampler = sampler

        if self.stats is not None:
            self.stats.reset()


class Trainer(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'freq_update',
            'frequency of model update',
            1)
        spec.addBoolOption(
            'save_first',
            'save first model',
            False)
        spec.addIntOption(
            'num_games',
            'number of games',
            1024)
        spec.addIntOption(
            'batchsize',
            'batch size',
            128)

        spec.merge(Evaluator.get_option_spec('trainer'))
        spec.merge(ModelSaver.get_option_spec())

        return spec

    @auto_import_options
    def __init__(self, option_map, verbose=False, actor_name="actor"):
        """Initialization for Trainer."""
        self.timer = RLTimer()
        self.verbose = verbose
        self.last_time = None
        self.evaluator = Evaluator(
            option_map,
            'trainer',
            verbose=verbose,
            actor_name=actor_name)
        self.saver = ModelSaver(option_map)
        self.counter = MultiCounter(verbose=verbose)
        self.just_update = False

    def actor(self, batch):
        """Actor.

        Get the model, forward the batch and get a distribution.

        Sample from it and act.

        Reply the message to game engine.

        Args:
            batch(dict): batch data

        Returns:
            reply_msg(dict):
                ``pi``: policy, ``a``: action, ``V``: value,
                `rv`: reply version, signatured by step
        """
        self.counter.inc("actor")
        return self.evaluator.actor(batch)

    def train(self, batch, *args, **kwargs):
        ''' Trainer.
        Get the model, forward the batch and update the weights.

        Args:
            batch(dict): batch data
        '''
        mi = self.evaluator.mi

        self.counter.inc("train")
        self.timer.record("batch_train")

        mi.zero_grad()
        res = self.rl_method.update(mi, batch,
                                    self.counter.stats, *args, **kwargs)
        if res["backprop"]:
            mi.update_weights()

        self.timer.record("compute_train")
        if self.counter.counts["train"] % self.options.freq_update == 0:
            # Update actor model
            # print("Update actor model")
            # Save the current model.
            if "actor" in mi:
                mi.update_model("actor", mi["model"])
            self.just_updated = True

        self.just_updated = False

    def episode_reset(self):
        ''' Reset stats '''
        self.counter.reset()
        self.timer.restart()

    def episode_start(self, i):
        ''' Called before each episode.

        Args:
            i(int): index in the minibatch
        '''
        self.evaluator.episode_start(i)

    def episode_summary(self, i, save=True):
        """Called after each episode. Print stats and summary.

        Also print arguments passed in.

        Args:
            i(int): index in the minibatch
        """
        prefix = "[%s][%d] Iter" % (
            str(datetime.now()), self.options.batchsize) + "[%d]: " % i
        print(prefix)
        if self.counter.counts["train"] > 0 and save:
            self.saver.feed(self.evaluator.mi["model"])

        print(
            "Command arguments:", ' '.join(map(str, self.options.parsed_args)))
        self.counter.summary(global_counter=i)
        print("")

        self.evaluator.episode_summary(i)

        self.episode_reset()

        return self.evaluator.mi["model"].step

    def setup(self, rl_method=None, mi=None, sampler=None):
        ''' Setup `RLMethod`, ModelInterface` and `Sampler`

        Args:
            rl_method(RLmethod)
            mi(`ModelInterface`)
            sample(`Sampler`)
        '''
        self.rl_method = rl_method
        self.evaluator.setup(mi=mi, sampler=sampler)
        if self.options.save_first:
            print("Save first: ")
            self.saver.feed(self.evaluator.mi["model"])
