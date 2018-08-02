import sys

import elf
import _elf as elf_C
import _elfgames_tutorial as test
from elf.options import auto_import_options, PyOptionSpec

spec = {}

spec["test"] = dict(
    input=["value"],
    reply=["reply"],
    # timeout_usec = 10,
)

class RunGC(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        test.toOptionsSpec(spec.getOptionSpec())
        spec.addIntOption(
            'gpu',
            'GPU id to use',
            -1)
        spec.addStrListOption(
            'parsed_args',
            'dummy option',
            [])
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.option_map = option_map

    def initialize(self):
        options = test.fromOptionsSpec(self.option_map.getOptionSpec())
        self.GC = elf_C.GameContext(options)
        self.game_obj = test.MyContext("test")
        self.game_obj.setGameContext(self.GC)
        self.wrapper = elf.GCWrapper(
            self.GC, self.game_obj, int(self.options.batchsize), spec, default_gpu=None, num_recv=2)
        self.wrapper.reg_callback("test", self.on_batch)

    def on_batch(self, batch):
        print("Receive batch: ", batch.smem.info(),
              ", curr_batchsize: ", str(batch.batchsize), sep='')
        reply = batch["value"][:, 0] * 2 + 1
        return dict(reply=reply)

    def start(self):
        self.wrapper.start()

    def run(self):
        self.wrapper.run()


if __name__ == '__main__':
    option_spec = PyOptionSpec()
    option_spec.merge(PyOptionSpec.fromClasses((RunGC,)))
    option_map = option_spec.parse()

    rungc = RunGC(option_map)
    rungc.initialize()
    rungc.start()
    for i in range(10):
        rungc.run()

    print("Stopping................")
    rungc.wrapper.stop()
