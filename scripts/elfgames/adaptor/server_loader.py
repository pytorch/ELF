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

        self.rs = elf.RemoteServers(elf.getNetOptions(opt, net_opt), ["actor", "train"])
        GC = elf.BatchReceiver(opt, self.rs)
        GC.setMode(elf.RECV_ENTRY)
        batchsize = opt.batchsize

        print("Batchsize: %d" % batchsize)

        width = 210 // 2
        height = 160 // 2

        e = GC.getExtractor()
        
        e.addField_float("s", batchsize, [batchsize, 3, height, width])
        e.addField_uint32_t("a", batchsize, [batchsize, 1])
        e.addField_float("V", batchsize, [batchsize])
        e.addField_float("pi", batchsize, [batchsize, 4])
        e.addField_float("last_r", batchsize, [batchsize])

        print(e.info())

        params = {
           "input_dim" : width * height * 3,
           "num_action" : 4
        }

        desc = {}
        desc["actor"] = dict(
            input=["s", "last_r"],
            reply=["pi", "a", "V"],
            batchsize=batchsize,
        )
        desc["train"] = dict(
            input=["s"],
            reply=[],
            batchsize=batchsize,
        )

        print("Init GC Wrapper")

        return GCWrapper(
            GC,
            None,
            batchsize,
            desc,
            num_recv=1,
            default_gpu=(self.options.gpu
                         if (self.options.gpu is not None and self.options.gpu >= 0)
                         else None),
            use_numpy=False, 
            params=params)

