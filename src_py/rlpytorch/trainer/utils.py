# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import os
from collections import defaultdict, deque, Counter
from datetime import datetime

from elf.options import auto_import_options, PyOptionSpec


class SymLink(object):
    def __init__(self, sym_prefix, latest_k=5):
        self.sym_prefix = sym_prefix
        self.latest_k = latest_k
        self.latest_files = deque()

    def feed(self, filename):
        self.latest_files.appendleft(filename)
        if len(self.latest_files) > self.latest_k:
            self.latest_files.pop()

        for k, name in enumerate(self.latest_files):
            symlink_file = self.sym_prefix + str(k)
            try:
                if os.path.exists(symlink_file):
                    os.unlink(symlink_file)
                os.symlink(name, symlink_file)
            except BaseException:
                print(
                    "Build symlink %s for %s failed, skipped" %
                    (symlink_file, name))


class ModelSaver(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'record_dir',
            'directory to record in',
            './record')
        spec.addStrOption(
            'save_prefix',
            'prefix of savefiles',
            'save')
        spec.addStrOption(
            'save_dir',
            'directory for savefiles',
            os.environ.get('save', './'))
        spec.addStrOption(
            'latest_symlink',
            'name for latest model symlink',
            'latest')
        spec.addIntOption(
            'num_games',
            'number of games',
            1024)
        spec.addIntOption(
            'batchsize',
            'batch size',
            128)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.save = (self.options.num_games == self.options.batchsize)
        if self.save and not os.path.exists(self.options.record_dir):
            os.mkdir(self.options.record_dir)
        if not os.path.exists(self.options.save_dir):
            os.mkdir(self.options.save_dir)

        self.symlinker = SymLink(
            os.path.join(
                self.options.save_dir,
                self.options.latest_symlink))

    def feed(self, model):
        basename = self.options.save_prefix + "-%d.bin" % model.step
        print("Save to " + self.options.save_dir)
        filename = os.path.join(self.options.save_dir, basename)
        print("Filename = " + filename)
        model.save(filename)
        # Create a symlink
        self.symlinker.feed(basename)


class ValueStats(object):
    def __init__(self, name=None):
        self.name = name
        self.reset()

    def feed(self, v):
        self.summation += v
        if v > self.max_value:
            self.max_value = v
            self.max_idx = self.counter
        if v < self.min_value:
            self.min_value = v
            self.min_idx = self.counter

        self.counter += 1

    def summary(self, info=None):
        info = "" if info is None else info
        name = "" if self.name is None else self.name
        if self.counter > 0:
            try:
                return "%s%s[%d]: avg: %.5f, min: %.5f[%d], max: %.5f[%d]" % (
                    info, name, self.counter, self.summation / self.counter,
                    self.min_value, self.min_idx, self.max_value, self.max_idx
                )
            except BaseException:
                return "%s%s[Err]:" % (info, name)
        else:
            return "%s%s[0]" % (info, name)

    def reset(self):
        self.counter = 0
        self.summation = 0.0
        self.max_value = -1e38
        self.min_value = 1e38
        self.max_idx = None
        self.min_idx = None


def topk_accuracy(output, target, topk=(1,)):
    """Computes the precision@k for the specified values of k"""
    maxk = max(topk)
    batch_size = target.size(0)

    _, pred = output.topk(maxk, 1, True, True)
    pred = pred.t()
    correct = pred.eq(target.view(1, -1).expand_as(pred))

    res = []
    for k in topk:
        correct_k = correct[:k].view(-1).float().sum(0)
        res.append(correct_k.mul_(100.0 / batch_size))
    return res


class MultiCounter(object):
    def __init__(self, verbose=False):
        self.last_time = None
        self.verbose = verbose
        self.counts = Counter()
        self.stats = defaultdict(lambda: ValueStats())
        self.total_count = 0

    def inc(self, key):
        if self.verbose:
            print("[MultiCounter]: %s" % key)
        self.counts[key] += 1
        self.total_count += 1

    def reset(self):
        for k in sorted(self.stats.keys()):
            self.stats[k].reset()

        self.counts = Counter()
        self.total_count = 0
        self.last_time = datetime.now()

    def summary(self, global_counter=None):
        this_time = datetime.now()
        if self.last_time is not None:
            print(
                "[%d] Time spent = %f ms" %
                (global_counter,
                 (this_time - self.last_time).total_seconds() * 1000))

        for key, count in self.counts.items():
            print("%s: %d/%d" % (key, count, self.total_count))

        for k in sorted(self.stats.keys()):
            v = self.stats[k]
            print(v.summary(info=str(global_counter) + ":" + k))
