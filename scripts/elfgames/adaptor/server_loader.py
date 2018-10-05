import os

from elf import GCWrapper
from elf.options import auto_import_options, PyOptionSpec

import _elf as elf

class Loader(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        elf.saveDefaultOptionsToArgs("", spec)
        elf.saveDefaultNetOptionsToArgs("", spec)
        spec.addIntOption(
            'gpu',
            'GPU id to use',
            -1)

        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.option_map = option_map

    def initialize(self):
        opt = elf.Options()
        net_opt = elf.NetOptions()

        opt.loadFromArgs("", self.option_map.getOptionSpec())
        net_opt.loadFromArgs("", self.option_map.getOptionSpec())

        GC = elf.BatchReceiver(opt, net_opt)
        GC.setMode(elf.RECV_ENTRY)
        batchsize = opt.batchsize

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
            None,
            batchsize,
            desc,
            num_recv=8,
            default_gpu=(self.options.gpu
                         if (self.options.gpu is not None and self.options.gpu >= 0)
                         else None),
            use_numpy=False)

