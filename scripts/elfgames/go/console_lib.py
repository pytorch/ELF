import inspect
import traceback
from collections import Counter
import time

from elf.options import auto_import_options, PyOptionSpec

def getTimeStamp():
    return int(time.time() * 1000)

def move2xy(v):
    if v.lower() == "pass":
        return -1, -1
    x = ord(v[0].lower()) - ord('a')
    # Skip 'i'
    if x >= 9:
        x -= 1
    y = int(v[1:]) - 1
    return x, y


def xy2move(x, y):
    if x == -1 and y == -1:
        return "pass"

    if x >= 8:
        x += 1
    return chr(x + 65) + str(y + 1)


def plot_plane(v):
    s = ""
    for j in range(v.size(1)):
        for i in range(v.size(0)):
            if v[i, v.size(1) - 1 - j] != 0:
                s += "o "
            else:
                s += ". "
        s += "\n"
    print(s)


def topk_accuracy2(batch, state_curr, topk=(1,)):
    pi = state_curr["pi"]
    import torch
    if isinstance(pi, torch.autograd.Variable):
        pi = pi.data
    score, indices = pi.sort(dim=1, descending=True)

    maxk = max(topk)
    topn_count = [0] * maxk

    for ind, gt in zip(indices, batch["offline_a"][0]):
        for i in range(maxk):
            if ind[i] == gt[0]:
                topn_count[i] += 1

    for i in range(maxk):
        topn_count[i] /= indices.size(0)

    return [topn_count[i - 1] for i in topk]


class GoConsole:
    def __init__(self, GC, evaluator):
        self.exit = False
        self.GC = GC
        self.board_size = GC.params["board_size"]
        self.evaluator = evaluator
        self.last_move_idx = None

    def move2action(self, v):
        if v.lower() == "pass":
            return self.board_size ** 2
        x, y = move2xy(v)
        return x * self.board_size + y

    def action2move(self, a):
        if a == self.board_size ** 2:
            return "pass"
        x = a // self.board_size
        y = a % self.board_size
        return xy2move(x, y)

    def check(self, batch):
        reply = self.evaluator.actor(batch)
        topk = topk_accuracy2(batch, reply, topk=(1, 2, 3, 4, 5))
        for i, v in enumerate(topk):
            self.check_stats[i] += v
        if sum(topk) == 0:
            self.check_stats[-1] += 1

    def actor(self, batch):
        reply = self.evaluator.actor(batch)
        return reply

    def showboard(self, batch):
        print(batch.game_obj.getGame(0).showBoard())

    def prompt(self, prompt_str, batch):
        if self.last_move_idx is not None:
            curr_move_idx = batch["move_idx"][0][0]
            if curr_move_idx - self.last_move_idx == 1:
                self.check(batch)
                self.last_move_idx = curr_move_idx
                return
            else:
                n = sum(self.check_stats.values())
                print("#Move: " + str(n))
                accu = 0
                for i in range(5):
                    accu += self.check_stats[i]
                    print("Top %d: %.3f" % (i, accu / n))
                self.last_move_idx = None

        self.showboard(batch)
        # Ask user to choose
        while True:
            if getattr(self, "repeat", 0) > 0:
                self.repeat -= 1
                cmd = self.repeat_cmd
            else:
                cmd = input(prompt_str)

            items = cmd.split()
            if len(items) < 1:
                print("Invalid input")

            c = items[0]
            reply = dict(pi=None, a=None, V=0)

            try:
                if c == 'p':
                    reply["a"] = self.move2action(items[1])
                    return reply
                elif c == 'c':
                    reply = self.evaluator.actor(batch)
                    return reply
                elif c == "s":
                    channel_id = int(items[1])
                    plot_plane(batch["s"][0][0][channel_id])
                elif c == "a":
                    reply = self.evaluator.actor(batch)
                    if "pi" in reply:
                        score, indices = reply["pi"].squeeze().sort(
                            dim=0, descending=True)
                        first_n = int(items[1])
                        for i in range(first_n):
                            print("%s: %.3f" %
                                  (self.action2move(indices[i]), score[i]))
                    else:
                        print("No key \"pi\"")
                elif c == "check":
                    print("Top %d" % self.check(batch))

                elif c == 'check2end':
                    self.check_stats = Counter()
                    self.check(batch)
                    self.last_move_idx = batch["move_idx"][0][0]
                    if len(items) == 2:
                        self.repeat = int(items[1])
                        self.repeat_cmd = "check2end_cont"
                    return

                elif c == "check2end_cont":
                    if not hasattr(self, "check_stats"):
                        self.check_stats = Counter()
                    self.check(batch)
                    self.last_move_idx = batch["move_idx"][0][0]
                    return

                elif c == "aug":
                    print(batch["aug_code"][0][0])
                elif c == "show":
                    self.showboard(batch)
                elif c == "dbg":
                    import pdb
                    pdb.set_trace()
                elif c == 'offline_a':
                    if "offline_a" in batch:
                        for i, offline_a in \
                                enumerate(batch["offline_a"][0][0]):
                            print(
                                "[%d]: %s" %
                                (i, self.action2move(offline_a)))
                    else:
                        print("No offline_a available!")
                elif c == "exit":
                    self.exit = True
                    return reply
                else:
                    print("Invalid input: " + cmd + ". Please try again")
            except Exception as e:
                print("Something wrong! " + str(e))

                '''
                elif c == "u":
                    batch.game_obj.undoMove(0)
                    self.showboard(batch)
                elif c == "h":
                    handicap = int(items[1])
                    batch.game_obj.applyHandicap(0, handicap)
                    self.showboard(batch)
                '''


class GoConsoleGTP:
    def on_protocol_version(self, batch, items, reply):
        return True, "2"

    def on_clear_board(self, batch, items, reply):
        reply["a"] = self.actions["clear"]
        return True, reply

    def on_name(self, batch, items, reply):
        return True, "DF2"

    def on_komi(self, batch, items, reply):
        # For now we just fix komi number.
        if items[1] != "7.5":
            return False, "We only support 7.5 komi for now"
        return True, None

    def on_boardsize(self, batch, items, reply):
        if items[1] != str(self.board_size):
            return (
                False,
                "We only support %dx%d board for now" % (
                    self.board_size, self.board_size)
            )
        return True, None

    def on_genmove(self, batch, items, reply):
        if len(items) >= 2:
            ret, msg = self.check_player(batch, items[1][0])
            if ret:
                reply["a"] = self.actions["skip"]
                return True, reply
            else:
                return False, msg
        else:
            reply["a"] = self.actions["skip"]
            return True, reply

    def on_play(self, batch, items, reply):
        ret, msg = self.check_player(batch, items[1][0])
        if ret:
            reply["a"] = self.move2action(items[2])
            return True, reply
        else:
            return False, msg

    def on_showboard(self, batch, items, reply):
        #self.showboard(batch)
        return True, None

    def on_final_score(self, batch, items, reply):
        final_score = self.get_final_score(batch)
        if final_score > 0:
            return True, "B+%.1f" % final_score
        else:
            return True, "W+%.1f" % (-final_score)

    def on_version(self, batch, items, reply):
        return True, "1.0"

    def on_exit(self, batch, items, reply):
        self.exit = True
        return True, reply

    def on_quit(self, batch, items, reply):
        return self.on_exit(batch, items, reply)

    def on_list_commands(self, batch, items, reply):
        msg = "\n".join(self.commands.keys())
        return True, msg

    def on_time_left(self, batch, items, reply):
        reply["timeleft"] = int(items[2]) * 1000
        reply["byoyomi"] = int(items[3]) 
        return True, None

    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addBoolOption(
            'auto_cmd',
            'Automatically replying the prompt with genmove',
            False)
        return spec

    @auto_import_options
    def __init__(self, spec):
        self.exit = False

    def setup(self, GC, evaluator):
        self.GC = GC
        self.board_size = GC.params["board_size"]
        self.evaluator = evaluator
        self.actions = {
            "skip": GC.params["ACTION_SKIP"],
            "pass": GC.params["ACTION_PASS"],
            "resign": GC.params["ACTION_RESIGN"],
            "clear": GC.params["ACTION_CLEAR"]
        }
        self.last_cmd = ""

        self.commands = {
            key[3:]: func
            for key, func in inspect.getmembers(
                self, predicate=inspect.ismethod)
            if key.startswith("on_")
        }

    def move2action(self, v):
        if v.lower() in self.actions:
            return self.actions[v.lower()]

        x, y = move2xy(v)
        return x * self.board_size + y

    def actor(self, batch):
        reply = self.evaluator.actor(batch)
        return reply

    def action2move(self, a):
        x = a // self.board_size
        y = a % self.board_size
        return xy2move(x, y)

    def showboard(self, batch):
        print(batch.game_obj.getGame(0).showBoard())

    def get_next_player(self, batch):
        return batch.game_obj.getGame(0).getNextPlayer()

    def get_last_move(self, batch):
        return batch.game_obj.getGame(0).getLastMove()

    def get_final_score(self, batch):
        return batch.game_obj.getGame(0).getLastScore()

    def check_player(self, batch, player):
        board_next_player = self.get_next_player(batch)
        if player.lower() != board_next_player.lower():
            return (
                False,
                ("Specified next player %s is not the same as the "
                 "next player %s on the board") % (
                    player, board_next_player
                )
            )
        else:
            return True, None

    def print_msg(self, ret, msg):
        print("%s %s\n" % (("=" if ret else "?"), msg))

    def prompt(self, prompt_str, batch):
        curr_timestamp = getTimeStamp()

        # Show last command results.
        if self.last_cmd == "play" or self.last_cmd == "clear_board":
            self.print_msg(True, "")
        elif self.last_cmd == "genmove":
            #print("Genmove spent: %d ms" % (curr_timestamp - self.curr_timestamp))
            self.print_msg(True, self.get_last_move(batch))

        self.last_cmd = ""

        while True:
            if self.options.auto_cmd:
                cmd = "genmove"
            else:
                cmd = input(prompt_str)

            items = cmd.split()
            if len(items) < 1:
                self.print_msg(False, "Invalid input")
                continue

            c = items[0]
            self.curr_timestamp = getTimeStamp()
            reply = dict(a=None, timestamp=self.curr_timestamp, timeleft=-1, byoyomi=-1)

            try:
                ret, msg = self.commands[c](batch, items, reply)
                self.last_cmd = c
                if not ret:
                    self.print_msg(False, msg)
                else:
                    if isinstance(msg, dict):
                        return msg
                    elif isinstance(msg, str):
                        self.print_msg(True, msg)
                    else:
                        self.print_msg(True, "")

            except Exception:
                print(traceback.format_exc())
                self.print_msg(False, "Invalid command")
