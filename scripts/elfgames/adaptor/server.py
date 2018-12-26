import sys
import os
import torch

from elf import GCWrapper, allocExtractor
from elf.options import auto_import_options, PyOptionSpec
from model import MyModel
import psutil

import _elf as elf

class RunGC(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        elf.saveDefaultOptionsToArgs("", spec)
        elf.saveDefaultNetOptionsToArgs("", spec)
        spec.addIntOption(
            'gpu',
            'GPU id to use',
            -1)
        spec.addStrListOption(
            "parsed_args",
            "dummy option",
            [])

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
        T = 6
        num_action = 4

        spec = {}
        spec["actor"] = dict(
            input=dict(s=("float", (3, height, width))),
            reply=dict(a=("int32_t", 1), pi=("float", num_action), V=("float", 1))
        )
        '''
        spec["train"] = dict(
            input=dict(s_=(T, 3, height, width), r_=(T, 1), a_=(T, 1), pi_=(T, num_action), V_=(T, 1)),
        )
        '''

        e = GC.getExtractor()
        desc = allocExtractor(e, batchsize, spec)

        params = {
           "input_dim" : width * height * 3,
           "num_action" : 4
        }

        print("Init GC Wrapper")
        has_gpu = self.options.gpu is not None and self.options.gpu >= 0

        self.wrapper = GCWrapper(
            GC, None, batchsize, desc, num_recv=1, default_gpu=(self.options.gpu if has_gpu else None),
            use_numpy=False, params=params)

        # wrapper.reg_callback("train", self.on_train)
        self.wrapper.reg_callback("actor", self.on_actor)
        self.model = MyModel(params)
        if has_gpu:
            self.model.cuda(self.options.gpu)
        # self.optim = torch.optimi.Adam(self.model.parameters())
        self.n = 0

    def on_actor(self, batch):
        res = self.model(batch)
        m = torch.distributions.Categorical(res["pi"].data)
        self.n += 1
        if self.n == 20:
            # gives a single float value
            #print(psutil.cpu_percent())
            # gives an object with many fields
            #print(psutil.virtual_memory())
            self.n = 0

        return dict(a=m.sample(), pi=res["pi"].data, V=res["V"].data)

    def on_train(self, batch):
        pass

def main():
    print(sys.version)
    print(torch.__version__)
    print(torch.version.cuda)
    print("Conda env: \"%s\"" % os.environ.get("CONDA_DEFAULT_ENV", ""))

    option_spec = PyOptionSpec()
    option_spec.merge(PyOptionSpec.fromClasses((RunGC,)))
    option_map = option_spec.parse()

    rungc = RunGC(option_map)
    rungc.initialize()

    num_batch = 10000000
    rungc.wrapper.start()

    for i in range(num_batch):
        rungc.wrapper.run()

    rungc.wrapper.stop()

if __name__ == '__main__':
    main()
