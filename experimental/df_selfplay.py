# Console for DarkForest

import os
import sys
import time
import re
from datetime import datetime

from rlpytorch import \
    Evaluator, load_env, ModelInterface

import torch

class Stats(object):
    def __init__(self):
        self.total_batchsize = 0
        self.total_sel_batchsize = 0
        self.actor_count = 0

    def feed(self, batch):
        # print("batchsize: %d" % batch.batchsize)
        self.total_sel_batchsize += batch.batchsize
        self.total_batchsize += batch.max_batchsize
        self.actor_count += 1

        if self.total_sel_batchsize >= 500000:
            print(datetime.now())
            print("Batch usage: %d/%d (%.2f%%)" %
                  (self.total_sel_batchsize, self.total_batchsize,
                      100.0 * self.total_sel_batchsize / self.total_batchsize))
            wr = batch.game_obj.getGameStats().getWinRateStats()
            black_wins = wr.getBlackWins()
            white_wins = wr.getWhiteWins()
            total_games = wr.getTotalGames()

            win_rate = (100.0 * black_wins / total_games
                        if total_games > 0
                        else 0.0)
            print(f'B/W: {black_wins}/{white_wins}. '
                  f'Black winrate: {win_rate:.2f} {total_games}')

            self.total_sel_batchsize = 0
            self.total_batchsize = 0
            print("Actor Count: %d" % self.actor_count)

name_matcher = re.compile(r"save-(\d+)")

def extract_ver(model_loader):
    name = os.path.basename(model_loader.options.load)
    m = name_matcher.match(name)
    return int(m.group(1))

def reload_model(model_loader, params, mi, actor_name, args):
    model = model_loader.load_model(params)

    if actor_name not in mi:
        mi.add_model(actor_name, model, cuda=(args.gpu >= 0), gpu_id=args.gpu)
    else:
        # mi["actor"].load(
        #     real_path, replace_prefix = [("resnet.module", "resnet")])
        mi.update_model(actor_name, model)
    mi[actor_name].eval()

def reload(mi, model_loader, params, args, root, ver, actor_name):
    if model_loader.options.load is None or model_loader.options.load == "":
        print("No previous model loaded, loading form " + root)
        real_path = os.path.join(root, "save-" + str(ver) + ".bin")
    else:
        this_root = os.path.dirname(model_loader.options.load)
        real_path = os.path.join(this_root, "save-" + str(ver) + ".bin")

    if model_loader.options.load != real_path:
        model_loader.options.load = real_path
        reload_model(model_loader, params, mi, actor_name, args)
    else:
        print("Warning! Same model, skip loading " + real_path)

def main():
    # Set game to online model.
    actors = ["actor_black", "actor_white"]
    additional_to_load = {
        ("eval_" + actor_name): (
            Evaluator.get_option_spec(name="eval_" + actor_name),
            lambda object_map, actor_name=actor_name: Evaluator(
                object_map, name="eval_" + actor_name,
                actor_name=actor_name, stats=None)
        )
        for i, actor_name in enumerate(actors)
    }
    additional_to_load.update({
        ("mi_" + name): (ModelInterface.get_option_spec(), ModelInterface)
        for name in actors
    })

    env = load_env(
        os.environ, num_models=2,
        additional_to_load=additional_to_load)

    GC = env["game"].initialize()

    stats = [Stats(), Stats()]

    # for actor_name, stat, model_loader, e in \
    #         zip(actors, stats, env["model_loaders"], evaluators):
    for i in range(len(actors)):
        actor_name = actors[i]
        stat = stats[i]
        e = env["eval_" + actor_name]

        print("register " + actor_name + " for e = " + str(e))
        e.setup(sampler=env["sampler"], mi=env["mi_" + actor_name])

        def actor(batch, e, stat):
            reply = e.actor(batch)
            stat.feed(batch)
            # eval_iters.stats.feed_batch(batch)
            return reply

        GC.reg_callback(actor_name,
                        lambda batch, e=e, stat=stat: actor(batch, e, stat))

    root = os.environ.get("root", "./")
    print("Root: \"%s\"" % root)
    args = env["game"].options
    global loop_end
    loop_end = False

    def game_start(batch):
        print("In game start")

        vers = [int(batch["black_ver"][0]), int(batch["white_ver"][0])]

        # Use the version number to load models.
        for model_loader, ver, actor_name in zip(
                env["model_loaders"], vers, actors):
            if ver >= 0:
                while True:
                    try:
                        old_model_file = model_loader.options.load
                        reload(
                            env["mi_" + actor_name], model_loader, GC.params,
                            args, root, ver, actor_name)
                        break
                    except BaseException:
                        import traceback
                        traceback.print_exc()
                        print("Restore back: model_loader.options.load = " + old_model_file)
                        model_loader.options.load = old_model_file
                        time.sleep(10)

    def game_end(batch):
        global loop_end
        # print("In game end")
        wr = batch.game_obj.getGameStats().getWinRateStats()
        black_wins = wr.getBlackWins()
        white_wins = wr.getWhiteWins()
        total_games = wr.getTotalGames()
        win_rate = 100.0 * black_wins / total_games if total_games > 0 else 0.0
        print("%s B/W: %d/%d. Black winrate: %.2f (%d)" %
                (str(datetime.now()), black_wins, white_wins, win_rate, total_games))

        if args.num_game_per_thread > 0 and total_games >= args.num_game_per_thread:
            print("#suicide_after_n_games: %d, total_games: %d" % (args.num_game_per_thread, total_games))
            loop_end = True

    GC.reg_callback_if_exists("game_start", game_start)
    GC.reg_callback_if_exists("game_end", game_end)

    # def episode_start(i):
    #     global GC
    #     GC.GC.setSelfplayCount(10000)
    #     evaluator.episode_start(i)

    GC.start()
    if args.eval_model_pair:
        if args.eval_model_pair.find(",") >= 0:
            black, white = args.eval_model_pair.split(",")
        else:
            black = extract_ver(env["model_loaders"][0])
            white = extract_ver(env["model_loaders"][1])

            # Force them to reload in the future.
            for model_loader, actor_name in zip(env["model_loaders"], actors):
                reload_model(model_loader, GC.params, \
                    env["mi_" + actor_name], actor_name, args)

        # We just use one thread to do selfplay.
        GC.game_obj.setRequest(int(black), int(white), 0.05, 1)

    for actor_name in actors:
        env["eval_" + actor_name].episode_start(0)

    while not loop_end:
        GC.run()

    GC.stop()


if __name__ == '__main__':
    print(sys.version)
    print(torch.__version__)
    print(torch.version.cuda)

    print("Conda env: \"%s\"" % os.environ.get("CONDA_DEFAULT_ENV", ""))
    main()
