# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from datetime import datetime
import time
from collections import defaultdict


class RLTimer:
    ''' A customized timer class'''

    def __init__(self):
        self.overall_counts = defaultdict(int)
        self.last_overall_mark = defaultdict(lambda: -1)
        self.restart()

    def restart(self):
        self.start_time = time.time()
        self.curr_time = datetime.now()
        self.durations = defaultdict(lambda: dict(duration=0, counter=0))

    def record(self, name):
        curr_time = datetime.now()
        self.durations[name]["duration"] += (curr_time -
                                             self.curr_time).total_seconds()
        self.durations[name]["counter"] += 1
        self.overall_counts[name] += 1
        self.curr_time = curr_time

    def print(self, nstep):
        final_time = time.time()
        total_duration = (final_time - self.start_time) * 1000.0 / nstep
        s = ", ".join(
            "%s: %.3f ms" %
            (name,
             d["duration"] *
                1000.0 /
                d["counter"]) for name,
            d in self.durations.items())
        return "Total: %.3f ms. " % total_duration + s

    def printInterval(self, name, nstep, callback):
        if self.checkPeriodicCondition(name, nstep):
            callback(self)
            self.restart()
            self.updatePeriodicCondition(name)

    def checkPeriodicCondition(self, name, nstep):
        curr_count = self.overall_counts[name]
        last_count = self.last_overall_mark[name]
        return curr_count > last_count and curr_count % nstep == 0

    def updatePeriodicCondition(self, name):
        self.last_overall_mark[name] = self.overall_counts[name]

    def getPeriodicValue(self, name):
        return self.overall_counts[name]
