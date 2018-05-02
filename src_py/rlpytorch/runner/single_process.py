# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import threading

from elf.options import auto_import_options, PyOptionSpec


class SingleProcessRun(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'num_minibatch',
            'number of minibatches',
            5000)
        spec.addIntOption(
            'num_cooldown',
            'Last #minibatches to refresh running mean/std for batchnorm '
            'in addition to the training stage',
            0)
        spec.addIntOption(
            'num_episode',
            'number of episodes',
            10000)
        spec.addBoolOption(
            'tqdm',
            'toggle tqdm visualization',
            False)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization for SingleProcessRun."""
        pass

    def setup(self, GC, episode_start=None, episode_summary=None,
              after_start=None, before_stop=None):
        ''' Setup for SingleProcessRun.

        Args:
            GC(`GameContext`): Game Context
            episode_start(func): operations to perform before each episode
            episode_summary(func): operations to summarize after each episode
            after_start(func): operations called after GC.start() but
                               before the main loop.
        '''
        self.GC = GC
        self.episode_summary = episode_summary
        self.episode_start = episode_start
        self.after_start = after_start
        self.before_stop = before_stop

    def run(self):
        """Main training loop. Initialize Game Context and looping the
        required episodes.

        Call episode_start and episode_summary before and after each episode
        if necessary.

        Visualize with a progress bar if ``tqdm`` is set.

        Print training stats after each episode.

        In the end, print summary for game context and stop it.
        """
        self.GC.start()
        if self.after_start is not None:
            self.after_start()

        for k in range(self.options.num_episode):
            if self.episode_start is not None:
                self.episode_start(k)
            if self.options.tqdm:
                import tqdm
                tq = tqdm.tqdm(total=self.options.num_minibatch, ncols=50)
            else:
                tq = None

            self.episode_counter = 0
            while self.episode_counter < self.options.num_minibatch:
                old_counter = self.episode_counter
                # Make sure if the callback function in GC.run() change the
                # counter, then the set value will not be added by 1.
                self.GC.run()
                self.episode_counter += 1
                diff = self.episode_counter - old_counter
                if tq is not None:
                    if diff < 0:
                        print(f'Diff negative: {old_counter} -> '
                              f'{self.episode_counter}')
                        tq = tqdm.tqdm(
                            total=self.options.num_minibatch, ncols=50)
                        tq.update(self.episode_counter)
                    else:
                        tq.update(diff)

            if self.options.num_cooldown > 0:
                print(f'Starting {self.options.num_cooldown} cooldown passes')
                self.cooldown_counter = 0
                while self.cooldown_counter < self.options.num_cooldown:
                    self.GC.run(
                        use_cooldown=True,
                        cooldown_count=self.cooldown_counter)
                    self.cooldown_counter += 1

            if self.episode_summary is not None:
                self.episode_summary(k)

        self.GC.printSummary()

        if self.before_stop is not None:
            self.before_stop()
        self.GC.stop()

    def set_episode_counter(self, counter):
        self.episode_counter = counter

    def inc_episode_counter(self, delta):
        self.episode_counter += delta

    def run_multithread(self):
        ''' Start training in a multithreaded environment '''
        def train_thread():
            for i in range(self.options.num_episode):
                for k in range(self.options.num_minibatch):
                    if self.episode_start is not None:
                        self.episode_start(k)

                    if k % 500 == 0:
                        print(
                            "Receive minibatch %d/%d" %
                            (k, self.options.num_minibatch))

                    self.GC.runGroup("train")

                # Print something.
                self.episode_summary(i)

        def actor_thread():
            while True:
                self.GC.runGroup("actor")

        self.GC.start()

        # Start the two threads.
        train_th = threading.Thread(target=train_thread)
        actor_th = threading.Thread(target=actor_thread)
        train_th.start()
        actor_th.start()
        train_th.join()
        actor_th.join()
