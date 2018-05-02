# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import import_options, PyOptionSpec


class EvalCount(object):
    ''' Eval Count. Run games and record required stats.'''

    def __init__(self):
        # All previous ids.
        self.ids = {}

        # id for old models.
        # If this variable is set, then do not count win_rate of ids_exclude.
        self.ids_exclude = set()
        self.summary_count = 0
        self.num_terminal = 0

    def reset(self):
        pass

    def _on_terminal(self, id, record):
        pass

    def reset_on_new_model(self):
        self.reset()
        self.ids_exclude.update(self.ids.keys())
        self.ids = dict()

    def feed(self, id, *args, **kwargs):
        # Game is running, not reaching terminal yet.
        # Register a game id.
        if id not in self.ids:
            self.ids[id] = 0

        self.ids[id] = self._on_game(id, self.ids[id], *args, **kwargs)

    def count_completed(self):
        return self.num_terminal

    def terminal(self, id):
        # If this game id ended and is in the exclude list, skip
        # It is not counted as the number of games completed.
        if id in self.ids_exclude:
            self.ids_exclude.remove(id)
            if id in self.ids:
                del self.ids[id]
            return

        if id in self.ids:
            self._on_terminal(id, self.ids[id])
            # This game is over, remove game id if it is already in ids
            del self.ids[id]
            self.num_terminal += 1
        # else:
        #    This should only happen when seq=0
        #    print("id=%s seq=%d, winner=%d" % (id, seq, winner))

    def summary(self):
        ret = self._summary()
        self.reset()
        self.num_terminal = 0
        self.summary_count += 1
        return ret

    def print_summary(self):
        summary = self.summary()
        for k, v in summary.items():
            print("%s: %s" % (str(k), str(v)))

    def feed_batch(self, batch, hist_idx=0):
        ids = batch["id"][hist_idx]
        last_terminals = batch["last_terminal"][hist_idx]
        last_r = batch["last_r"][hist_idx]

        for batch_idx, (id, last_terminal) in enumerate(
                zip(ids, last_terminals)):
            self.feed(id, last_r[batch_idx])
            if last_terminal:
                self.terminal(id)


class RewardCount(EvalCount):
    ''' Class to accumulate rewards achieved'''

    def __init__(self):
        super(RewardCount, self).__init__()
        self.reset()

    def reset(self):
        self.n = 0
        self.sum_reward = 0

    def _on_terminal(self, id, record):
        self.sum_reward += record
        self.n += 1

    def _on_game(self, id, record, reward, seq=None):
        return record + reward

    def _summary(self):
        str_reward = "[%d] Reward: %.2f/%d" % (
            self.summary_count,
            float(self.sum_reward) / (self.n + 1e-10),
            self.n
        )
        return dict(str_reward=str_reward)


class WinRate(EvalCount):
    ''' Class to accumulate game results to win rate'''

    def __init__(self):
        super(WinRate, self).__init__()
        self.total_win_count = 0
        self.total_lose_count = 0
        self.summary_count = 0
        self.highest_win_rate = -1.0
        self.highest_win_rate_idx = -1

    def reset(self):
        self.win_count = 0
        self.lose_count = 0

    def _on_game(self, id, record, final_reward, seq=None):
        if final_reward > 0.5:
            self.win_count += 1
            self.total_win_count += 1
        elif final_reward < -0.5:
            self.lose_count += 1
            self.total_lose_count += 1

    def _summary(self):
        total = self.win_count + self.lose_count
        win_rate = self.win_count / (total + 1e-10)
        new_record = False
        if win_rate > self.highest_win_rate:
            self.highest_win_rate = win_rate
            self.highest_win_rate_idx = self.summary_count
            new_record = True

        str_win_rate = (
            f'[{self.summary_count}] Win rate: {win_rate:.3f} '
            f'[{self.win_count}/{self.lose_count}/{total}], '
            f'Best win rate: {self.highest_win_rate:.3f} '
            f'[{self.highest_win_rate_idx}]'
        )

        total = self.total_win_count + self.total_lose_count
        str_acc_win_rate = "Accumulated win rate: %.3f [%d/%d/%d]" % (
            self.total_win_count / (total + 1e-10),
            self.total_win_count, self.total_lose_count, total
        )

        return dict(
            new_record=new_record,
            count=self.summary_count,
            best_win_rate=self.highest_win_rate,
            str_win_rate=str_win_rate,
            str_acc_win_rate=str_acc_win_rate,
        )

    def win_count(self): return self.total_win_count

    def lose_count(self): return self.total_lose_count

    def total_winlose_count(
        self): return self.total_win_count + self.total_lose_count

    def winlose_count(self): return self.win_count + self.lose_count


class Stats(EvalCount):
    @classmethod
    def get_option_spec(cls, stats_name=''):
        spec = PyOptionSpec()
        spec.addStrOption(
            stats_name + '_stats',
            'type of stat to report (rewards or winrate)',
            '')
        return spec

    def __init__(self, option_map, stats_name=''):
        """Initialization for Stats."""
        import_options(self, option_map, self.get_option_spec(stats_name))

        self.name = stats_name + "_stats"
        self.collector = None

        self.stats_name = getattr(self.options, self.name)
        if self.stats_name == "rewards":
            self.collector = RewardCount()
        elif self.stats_name == "winrate":
            self.collector = WinRate()
        else:
            self.collector = None
            print("Stats: Name " + str(self.stats_name) + " is not known!")
            # raise ValueError(
            #     "Name " + str(self.stats_name) + " is not known!")

    def is_valid(self):
        return self.collector is not None

    def feed(self, id, *args, **kwargs):
        self.collector.feed(id, *args, **kwargs)

    def count_completed(self):
        return self.collector.count_completed()

    def reset_on_new_model(self):
        self.collector.reset_on_new_model()

    def terminal(self, id):
        return self.collector.terminal(id)

    def reset(self):
        self.collector.reset()

    def summary(self):
        return self.collector.summary()

    def print_summary(self):
        self.collector.print_summary()

    def feed_batch(self, batch, hist_idx=0):
        return self.collector.feed_batch(batch, hist_idx=hist_idx)
