# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from collections import defaultdict, deque


class HistState:
    def __init__(self, T, init_state_func=None):
        self.hs = defaultdict(lambda: deque())
        self.T = T
        self.init_state_func = init_state_func

    def preprocess(self, ids, seqs):
        for id, seq in zip(ids, seqs):
            q = self.hs[id]
            if seq == 0:
                # clear the queue (which might contain old states of the last
                # game)
                q.clear()
                if self.init_state_func is not None:
                    q.append(self.init_state_func())

    def feed(self, ids, hiddens):
        '''
        h[0] is the oldest element (left-most),
        h[-1] is the newest element (right-most)
        '''

        for id, h in zip(ids, hiddens):
            q = self.hs[id]
            # Put the newest element from the right.
            q.append(h)
            # Pop the oldest element from the left.
            if len(q) > self.T:
                q.popleft()

    def _get_batch(self, ids, t, default=None):
        list_output = False
        if default is None:
            templ = self.hs[ids[0]][t]
            if isinstance(templ, (dict, list)):
                data = []
                list_output = True
            else:
                data = templ.clone().resize_(len(ids), templ.size(0))
        else:
            data = default.clone()
        for i, id in enumerate(ids):
            if id in self.hs:
                if not list_output:
                    data[i, :] = self.hs[id][t]
                else:
                    data.append(self.hs[id][t])
        return data

    def newest(self, ids, t, default=None):
        return self._get_batch(ids, -t - 1, default=default)

    def oldest(self, ids, t, default=None):
        return self._get_batch(ids, t, default=default)

    def map(self, ids, func):
        hs = self.hs[ids[0]][0].clone()
        hs.resize_(len(ids), *list(hs.size()))
        for t in range(self.T):
            # Collect the data.
            for i, id in enumerate(ids):
                if t < len(self.hs[id]):
                    hs[i, :] = self.hs[id][t]

            output = func(hs)

            # Update the state.
            for id, h in zip(ids, output):
                if t < len(self.hs[id]):
                    self.hs[id][t] = h
