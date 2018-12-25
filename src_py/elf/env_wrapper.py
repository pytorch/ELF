import _elf as elf
from .utils_elf import allocExtractor, NameConverter, Allocator

class EnvWrapper(object):
    def __init__(self):
        opt = elf.Options()
        net_opt = elf.NetOptions()
        net_opt.port = 5566
        self.rc = elf.RemoteClients(elf.getNetOptions(opt, net_opt), ["actor"])

        self.wrapper = elf.EnvSender(self.rc)
        self.converter = NameConverter()

    def alloc(self, spec, batchsize=1):
        e = self.wrapper.getExtractor()
        self.desc = allocExtractor(e, batchsize, spec)
        print(e.info())

        self.allocator = Allocator(self.wrapper, None, batchsize, self.desc,
            use_numpy=True, default_gpu=None, num_recv=1)

        inputs = set()
        for name, desc_one in self.desc.items():
            inputs.update(desc_one["input"])

        self.wrapper.setInputKeys(inputs)

        # Only pick the first buffer.
        inputs, replies = self.allocator.getMem()
        mems = dict()
        mems.update(inputs)
        mems.update(replies)

        return mems
