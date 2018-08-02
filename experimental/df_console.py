# Console for DarkForest
import os
from rlpytorch import Evaluator, load_env
from console_lib import GoConsoleGTP

import psutil
pid = os.getpid()

if __name__ == '__main__':
    additional_to_load = {
        'evaluator': (
            Evaluator.get_option_spec(),
            lambda object_map: Evaluator(object_map, stats=None)),
        'console': (
            GoConsoleGTP.get_option_spec(),
            lambda object_map: GoConsoleGTP(object_map))
    }

    # Set game to online model.
    env = load_env(
        os.environ,
        overrides=dict(
            additional_labels=['aug_code', 'move_idx'],
        ),
        additional_to_load=additional_to_load)
    evaluator = env['evaluator']

    GC = env["game"].initialize()
    console = env["console"]

    model_loader = env["model_loaders"][0]
    model = model_loader.load_model(GC.params)
    gpu = model_loader.options.gpu
    use_gpu = gpu is not None and gpu >= 0

    mi = env['mi']
    mi.add_model("model", model)
    # mi.add_model(
    #     "actor", model,
    #     copy=True, cuda=use_gpu, gpu_id=gpu)
    mi.add_model("actor", model)
    mi["model"].eval()
    mi["actor"].eval()

    console.setup(GC, evaluator)
    def human_actor(batch):
        #py = psutil.Process(pid)
        #memoryUse = py.memory_info()[0]/2.**30  # memory use in GB...I think
        #print('memory use:', memoryUse)
        return console.prompt("", batch)

    def actor(batch):
        return console.actor(batch)

    def train(batch):
        console.prompt("DF Train> ", batch)

    evaluator.setup(sampler=env["sampler"], mi=mi)

    GC.reg_callback_if_exists("actor_black", actor)
    GC.reg_callback_if_exists("human_actor", human_actor)
    GC.reg_callback_if_exists("train", train)
    GC.start()
    # TODO: For now fixed resign threshold to be 0.05. Will add a switch
    GC.game_obj.setRequest(mi["actor"].step, -1, 0.05, -1)

    evaluator.episode_start(0)

    while True:
        GC.run()
        if console.exit:
            break

    GC.stop()
