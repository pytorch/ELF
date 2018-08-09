import os

from elf import GCWrapper
from elf.options import auto_import_options, PyOptionSpec

import _elf as elf
import _elfgames_tutorial_distri as tutorial

class Loader(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        tutorial.getPredefined(spec.getOptionSpec())
        
        spec.addIntOption(
            'server_dummy',
            'Some dummy arguments',
            -1)
        spec.addStrOption(
            'server_dummy2',
            'some string dummy arguments',
            '')
        spec.addBoolOption(
            "server_dummy3",
            "Some boolean dummy arguments",
            True)

        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.option_map = option_map

    def initialize(self):
        job_id = os.environ.get("job_id", "local")
        opt = go.getOpt(self.option_map.getOptionSpec(), job_id)

        GC = elf.GameContext(opt.base)
        game_obj = tutorial.Server(opt)
        game_obj.setGameContext(GC)
        params = game_obj.getParams()

        batchsize = opt.base.batchsize

        desc = {}
        desc["actor"] = dict(
            input=["s"],
            reply=["a", "V"],
            batchsize=batchsize,
        )
        desc["train"] = dict(
            input=["s"],
            reply=[],
            batchsize=batchsize,
        )

        return GCWrapper(
            GC,
            game_obj,
            batchsize,
            desc,
            num_recv=8,
            default_gpu=(self.options.gpu
                         if (self.options.gpu is not None and self.options.gpu >= 0)
                         else None),
            use_numpy=False,
            params=params,
            verbose=self.options.parameter_print)

