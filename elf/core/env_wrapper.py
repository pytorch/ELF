import _elf as elf
from .utils_elf import allocExtractor, Allocator


def allocExtractor(e, batchsize, spec):
    '''
    Input format:
    spec["train"] = {
        input={
            "s": { "float", (state_size) },
            "a": { "uint32_t", (action_size) }
        }
    }

    '''
    desc = {}
    for name, spec_one in spec.items():
        bs = spec_one.get("batchsize", batchsize)
        desc[name] = dict(batchsize=bs)
        for sel in ["input", "reply"]:
            if sel not in spec_one:
                continue
            desc[name][sel] = list()
            for entry, spec in spec_one[sel].items():
                func_name = "addField_" + spec[0]
                sz = [bs] + (list(spec[1]) if isinstance(spec[1], (tuple,list)) else [spec[1]])

                eval(f"e.{func_name}(\"{entry}\", {bs}, {sz})")
                desc[name][sel].append(entry)

    return desc


class EnvWrapper:
    def __init__(self):
        opt = elf.Options()
        net_opt = elf.NetOptions()
        net_opt.port = 5566
        self.rc = elf.RemoteClients(elf.getNetOptions(opt, net_opt), ["actor"])

        self.wrapper = elf.EnvSender(self.rc)
        # self.converter = NameConverter()

    def alloc(self, spec, batchsize=1):
        e = self.wrapper.getExtractor()
        self.desc = allocExtractor(e, batchsize, spec)
        print(e.info())

        self.allocator = Allocator(
            self.wrapper,
            None,
            batchsize,
            self.desc,
            use_numpy=True,
            default_gpu=None,
            num_recv=1)

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
