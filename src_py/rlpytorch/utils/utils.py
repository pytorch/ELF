# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import math
import queue
from collections import defaultdict, Counter
from datetime import datetime

import numpy as np
import torch
import torch.multiprocessing as _mp
import msgpack
import msgpack_numpy

from .size_utils import total_size


mp = _mp.get_context('spawn')
msgpack_numpy.patch()


def dumps(obj):
    return msgpack.dumps(obj, use_bin_type=True)


def loads(buf):
    return msgpack.loads(buf)


def npimg_convert(img):
    img = img.astype('float32') / 255.0
    img = np.transpose(img, (2, 0, 1))
    return img


def check_done_flag(done_flag):
    if done_flag is not None:
        with done_flag.get_lock():
            return done_flag.value
    return False


def islambda(v):
    def LAMBDA(): return 0
    return isinstance(v, type(LAMBDA)) and v.__name__ == LAMBDA.__name__


def queue_get(q, done_flag=None, fail_comment=None):
    if done_flag is None:
        return q.get()
    done = False
    while not done:
        try:
            return q.get(True, 0.01)
        except queue.Empty:
            if fail_comment is not None:
                print(fail_comment)
            if check_done_flag(done_flag):
                done = True
    # Return
    return None


def queue_put(q, item, done_flag=None, fail_comment=None):
    if done_flag is None:
        q.put(item)
        return True
    done = False
    while not done:
        try:
            q.put(item, True, 0.01)
            return True
        except queue.Full:
            if fail_comment is not None:
                print(fail_comment)
            if check_done_flag(done_flag):
                done = True
    return False


class Switch:
    def __init__(self, val=True):
        self.val = mp.Value("b", val)
        self.lock = mp.Lock()

    def get(self):
        with self.lock:
            return self.val.value

    def set(self, v):
        with self.lock:
            self.val.value = v


class Timer:
    def __init__(self):
        self.reset()

    def __call__(self, name):
        self.curr_name = name
        return self

    def __enter__(self):
        self.before[self.curr_name] = datetime.now()

    def __exit__(self, t, value, traceback):
        after = datetime.now()
        elapsed = (after - self.before[self.curr_name]).total_seconds() * 1000
        self.records[self.curr_name][0] += elapsed
        self.records[self.curr_name][1] += 1

    def summary(self):
        rets = []
        for name, record in self.records.items():
            cumtime, count = record
            aver_time = float(cumtime) / count
            rets.append("[%s] %.3f ms [%d]" % (name, aver_time, count))
        return rets

    def reset(self):
        self.records = defaultdict(lambda: [0, 0])
        self.before = {}


class CategoryCounter:
    def __init__(self, name=None):
        self.name = name
        self.reset()

    def reset(self):
        self.counter = Counter()

    def feed(self, data):
        for v in data:
            self.counter[v] += 1

    def summary(self, info=""):
        n = sum(self.counter.values())
        prompt = "[%s] n = %d " % (info, n)
        if n > 0:
            return prompt + "\n" + \
                "\n".join(["  \"%s\": %d (%.2lf%%)" % (k, v, 100.0 * v / n)
                           for k, v in self.counter.items()])
        else:
            return prompt


class DelayedStats:
    def __init__(self, prefix, max_delay=5):
        ''' self.entries[key][t] gives the value of key at time t '''
        self.prefix = prefix
        self.max_delay = max_delay
        self.reset()

    def reset(self):
        # self.entries[key][t_id] -> value
        self.entries = defaultdict(dict)
        self.predicted_entries = [
            defaultdict(dict) for i in range(
                self.max_delay)]
        self.baseline_entries = [
            defaultdict(dict) for i in range(
                self.max_delay)]

    def feed(self, ts, ids, curr, pred_diff, curr_cb=None, diff_cb=None):
        """Check keys in curr and pred, if there is any key starts with 'fa_',
        collect them and compare against each other.

        For example (suppose we are at time t):

            num_unit_T2: predicted difference: curr["num_unit"] (at time t + 2)
                                               - curr["num_unit"] (at time t).
        """
        # curr[key][i] -> value, ids[i] -> id
        for k, v in curr.items():
            if not k.startswith(self.prefix):
                continue

            key = k[len(self.prefix):]
            history = self.entries[key]
            history.update({
                str(t) + "_" + str(d): (v[i] if not curr_cb else curr_cb(v[i]))
                for i, (t, d) in enumerate(zip(ts, ids))
            })

        for k, v in pred_diff.items():
            if not k.startswith(self.prefix):
                continue
            key = k[len(self.prefix):]
            idx = key.rfind("_")
            delay = int(key[idx + 2:])
            if delay >= self.max_delay:
                continue

            key = key[:idx]

            # Save it
            history = self.predicted_entries[delay][key]
            history.update({
                str(t + delay) + "_" + str(d): (
                    self.entries[key][str(t) + "_" + str(d)] +
                    (v[i] if not diff_cb else diff_cb(v[i]))
                ) for i, (t, d) in enumerate(zip(ts, ids))
            })

            history2 = self.baseline_entries[delay][key]
            history2.update({
                str(t + delay) + "_" + str(d):
                self.entries[key][str(t) + "_" + str(d)]
                for t, d in zip(ts, ids)
            })

    def _compare_history(self, h1, h2):
        summation = 0
        counter = 0
        # h1[t_id] -> val
        for t_id, v1 in h1.items():
            if not (t_id in h2):
                continue
            v2 = h2[t_id]
            summation += (v1 - v2) ** 2
            counter += 1
        return summation / (counter + 1e-8), counter

    def summary(self, info=""):
        for k, v in self.entries.items():
            for i in range(1, self.max_delay):
                # Difference
                avgMSE, counter = self._compare_history(
                    self.predicted_entries[i][k], v)
                avgMSE_bl, counter = self._compare_history(
                    self.baseline_entries[i][k], v)
                print(
                    "[%s][%s_T%d] RMS: %.4lf, Baseline: %.4lf [cnt=%d]" %
                    (info, k, i, math.sqrt(avgMSE), math.sqrt(avgMSE_bl),
                     counter)
                )


def print_dict(prompt, d, func=str, tight=False):
    dem = ", " if tight else "\n"
    print(prompt, end='')
    if not tight:
        print("")
    print(dem.join(["%s: %s" % (k, func(d[k])) for k in sorted(d.keys())]))
    if not tight:
        print("")


def print_dict2(prompt, d1, d2, func=lambda x, y: str(x) + "_" + str(y)):
    print(prompt)
    items = []
    for k in sorted(d1.keys()):
        if not (k in d2):
            continue
        v1 = d1[k]
        v2 = d2[k]
        items.append("%s: %s" % (k, func(v1, v2)))

    print("\n".join(items))
    print("")


def is_viskey(k):
    return k.startswith("_") or k.startswith("fa_")


def get_avg_str(l):
    return ", ".join(["[%d]: %.2lf [cnt=%d]" % (i, math.sqrt(
        v / (c + 1e-10)), c) for i, (v, c) in enumerate(zip(l[::2], l[1::2]))])


def get_avg_str2(l, l_bl):
    items = []
    for i, (v1, c1, v2, c2) in enumerate(
            zip(l[::2], l[1::2], l_bl[::2], l_bl[1::2])):
        r1 = math.sqrt(v1 / (c1 + 1e-10))
        r2 = math.sqrt(v2 / (c2 + 1e-10))
        items.append("[%d]: %.2lf=%.2lf/%.2lf(%d)" %
                     (i, r1 / (r2 + 1e-10), r1, r2, c1))
    return ", ".join(items)


class ForwardTracker:
    def __init__(self, max_delay=6):
        # prediction[key][t] -> value
        self.max_delay = max_delay
        self.sum_sqr_err = defaultdict(lambda: [0] * (2 * self.max_delay))
        self.sum_sqr_err_bl = defaultdict(lambda: [0] * (2 * self.max_delay))
        self.reset()

    def reset(self):
        self.prediction = defaultdict(
            lambda: defaultdict(lambda: {
                "pred": [0] * self.max_delay,
                "baseline": [0] * self.max_delay
            })
        )

    def feed(self, batch_states, curr_batch, forwarded):
        # Dump all entries with _, and with fa_
        if curr_batch is None and forwarded is None:
            state_info = {
                k: v for k,
                v in batch_states[0].items() if is_viskey(k)}
            print_dict("[batch states]: ", state_info, tight=True)
            return

        batch_info = {
            k: v if isinstance(
                v,
                (int,
                 float,
                 str)) else v[0] for k,
            v in curr_batch.items() if is_viskey(k)}
        fd_info = {k: v.data[0] for k, v in forwarded.items() if is_viskey(k)}

        t0 = batch_info["_seq"]
        additional_info = {}
        used_fd_info = defaultdict(lambda: [0] * self.max_delay)

        for k, v in batch_info.items():
            pred = self.prediction[k]
            # If there is prediction of the current value, also show them.
            if t0 in pred:
                cp = pred[t0]
                # Also compute th error.
                for delay, p in enumerate(cp["pred"]):
                    self.sum_sqr_err[k][2 * delay] += (p - v) ** 2
                    self.sum_sqr_err[k][2 * delay + 1] += 1

                for delay, p in enumerate(cp["baseline"]):
                    self.sum_sqr_err_bl[k][2 * delay] += (p - v) ** 2
                    self.sum_sqr_err_bl[k][2 * delay + 1] += 1

                additional_info[k + "_pred"] = ", ".join([
                    "[%d] %.2f" % (delay, p)
                    for delay, p in enumerate(cp["pred"]) if delay != 0
                ])
                additional_info[k + "_bl"] = ", ".join([
                    "[%d] %.2f" % (delay, p)
                    for delay, p in enumerate(cp["baseline"]) if delay != 0
                ])
                del pred[t0]

            for t in range(1, self.max_delay):
                k_f = k + "_T" + str(t)
                if not (k_f in fd_info):
                    continue
                predictions = pred[t0 + t]
                predictions["pred"][t] = fd_info[k_f] + v
                predictions["baseline"][t] = v
                used_fd_info[k][t] = fd_info[k_f]

        batch_info.update(additional_info)
        used_fd_info = {
            k: ", ".join([
                "[%d] %.2f" % (i, vv) for i, vv in enumerate(v) if i != 0
            ])
            for k, v in used_fd_info.items()
        }

        # print("--------------")
        # print_dict2("[statistics]:", self.sum_sqr_err, self.sum_sqr_err_bl,
        #     func=get_avg_str2)
        # print_dict("[batch after _make_batch]: ", batch_info)
        # print_dict("[state_curr after forward]: ", used_fd_info)


class SeqStats:
    def __init__(self, name="seq", seq_limits=None):
        # Stats.
        self.stats_seq = Counter()
        self.clear_stats()
        self.name = name

        if seq_limits is None:
            self.limits = [
                1,
                100,
                200,
                300,
                400,
                500,
                600,
                700,
                800,
                900,
                1000,
                1200,
                1400,
                1600,
                1800,
                2000,
                2500,
                3000,
                4000,
                5000,
                float("inf")]
        else:
            self.limits = seq_limits
            if not np.isinf(self.limits[-1]):
                self.limits.append(float("inf"))

    def feed(self, seqs):
        for seq_num in seqs:
            bin_idx = None
            for i, limit in enumerate(self.limits[1:]):
                if int(seq_num) < limit:
                    bin_idx = i
                    break
            if seq_num > self.max_seq:
                self.max_seq = seq_num
            if seq_num < self.min_seq:
                self.min_seq = seq_num

            name = "[" + str(self.limits[bin_idx]) + ", " + \
                str(self.limits[bin_idx + 1]) + ")"
            self.stats_seq[name] += 1

    def print_stats(self, reset=False):
        total_counts = sum(self.stats_seq.values())
        if total_counts > 0:
            print(
                "Distribution of %s [min = %d / max = %d / #count = %d]:" %
                (self.name, self.min_seq, self.max_seq, total_counts))
            s = ""
            for r in sorted(self.stats_seq.keys(),
                            key=lambda x: float(x.split(",")[0][1:])):
                s += "%s: %d [%.2lf%%]\n" % (
                    r,
                    self.stats_seq[r],
                    100.0 * self.stats_seq[r] / total_counts)
            print(s)
        else:
            print(
                "Distribution of %s [#count = %d]:" %
                (self.name, total_counts))

        if reset:
            self.clear_stats()

    def clear_stats(self):
        self.stats_seq.clear()
        self.max_seq = 0
        self.min_seq = float('inf')


def agent2sender(agent_name):
    return agent_name[:-5].encode('ascii')


def sender2agent(sender, i):
    return sender + "-%04d" % i


def npimgs2cudatensor(imgs):
    imgs = torch.from_numpy(imgs)
    imgs = imgs.float().div(255)
    imgs = imgs.transpose(0, 1).transpose(0, 2).contiguous()
    imgs.cuda()
    return imgs


def print_binary(m):
    # Print a binary matrix.
    if len(m.size()) != 2:
        print("Err! cannot print matrix of size " + str(m.size()))
        return
    s = ""
    for i in range(m.size(0)):
        for j in range(m.size(2)):
            if m[i, j] != 0:
                s += "x"
            else:
                s += "."
        s += "\n"
    print(s)


def get_total_size(o):
    def get_tensor_size(t):
        return t.numel() * t.element_size()

    tensor_objects = [
        torch.ByteTensor,
        torch.FloatTensor,
        torch.DoubleTensor,
        torch.IntTensor,
        torch.LongTensor,
        torch.cuda.ByteTensor,
        torch.cuda.FloatTensor,
        torch.cuda.DoubleTensor,
        torch.cuda.IntTensor,
        torch.cuda.LongTensor,
    ]

    obj_handlers = {obj: get_tensor_size for obj in tensor_objects}
    return total_size(o, obj_handlers=obj_handlers)
