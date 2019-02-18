from collections import defaultdict
import numpy as np
import torch
import _elf as elf
from .batch import Batch


class NameConverter:
    def __init__(self):
        self._c2torch = {
            'int32_t': torch.IntTensor,
            'uint32_t': torch.IntTensor,
            'int64_t': torch.LongTensor,
            'uint64_t': torch.LongTensor,
            'float': torch.FloatTensor,
            'unsigned char': torch.ByteTensor,
            'char': torch.ByteTensor
        }
        self._c2numpy = {
            'int32_t': np.dtype('i4'),
            'uint32_t': np.dtype('i4'),
            'int64_t': np.dtype('i8'),
            'uint64_t': np.dtype('i8'),
            'float': np.dtype('f4'),
            'unsigned char': np.dtype('byte'),
            'char': np.dtype('byte')
        }

        self._torch2c = dict()
        for k, v in self._c2torch.items():
            self._torch2c[v.dtype] = k

        self._numpy2c = dict()
        for k, v in self._c2numpy.items():
            self._numpy2c[v] = k

    def torch2c(self, v):
        return self._torch2c[v.dtype]

    def c2torch(self, s):
        return self._c2torch[s]

    def c2numpy(self, s):
        return self._c2numpy[s]

    def numpy2c(self, v):
        return self._numpy2c[v.dtype]

    def get_info(self, v, type_name=None):
        info = elf.PointerInfo()
        if isinstance(v, torch.Tensor):
            info.stride = [i * v.element_size() for i in v.stride()]
            info.p = v.data_ptr()
            if type_name is None:
                info.type = self.torch2c(v)
            else:
                info.type = type_name
        elif isinstance(v, np.ndarray):
            # Return pointer, size and byte_size
            info.p = v.ctypes.data
            info.stride = v.strides
            if type_name is None:
                info.type = self.numpy2c(v)
            else:
                info.type = type_name
        else:
            raise NotImplementedError
        return info


class Allocator:
    """A wrapper class for batch data"""
    def __init__(self,
                 game_context,
                 spec,
                 default_batchsize,
                 default_device,
                 num_recv):
        self.game_context = game_context
        self.batches = []
        self.target2idx = defaultdict(list)
        self.idx2target = dict()
        self.converter = NameConverter()

        for target, target_spec in spec.items():
            # print('%s: %s' % (name, v))
            input_ = target_spec.get('input', [])
            reply = target_spec.get('reply', [])
            batchsize = target_spec.get('batchsize', default_batchsize)
            device = target_spec.get('device', default_device)
            timeout = target_spec.get('timeout_usec', 0)
            keys = list(set(input_ + reply))
            # print('SharedMem: \'%s\', keys: %s' % (name, str(keys)))

            smem_opts = elf.SharedMemOptions(target, batchsize)
            smem_opts.setTimeout(timeout)

            for _ in range(num_recv):
                smem = game_context.allocateSharedMem(smem_opts, keys)
                # spec_local = dict()
                input2data = {}
                reply2data = {}
                for field in keys:
                    assert smem[field] is not None, f'{field} is not in keys = {str(keys)}'
                    data = self._alloc(smem[field])
                    if field in input_:
                        input2data[field] = data
                    if field in reply:
                        reply2data[field] = data

                self.batches.append({
                    'input': input2data,
                    'reply': reply2data,
                    'device': device
                })

                idx = smem.getSharedMemOptions().idx()
                self.target2idx[target].append(idx)
                self.idx2target[idx] = target

    # def getMem(self, recv_idx=0):
    #     input_ = dict()
    #     reply = dict()
    #     for key, indices in self.target2idx.items():
    #         idx = indices[recv_idx]
    #         b = self.batches[idx]
    #         input_.update(b['input'])
    #         reply.update(b['reply'])

    #     return input_, reply

    def get_input_batch(self, idx, actual_batchsize):
        batch = Batch(self.batches[idx]['input']).first_k(actual_batchsize)
        device = self.batches[idx]['device']
        if device is not None:
            batch = batch.to(device)

        return batch
        #     picked = picked.cpu2device(device)
        # picked.batchsize = batchsize
        # picked.device = device
        # return picked

    def get_reply_batch(self, idx, actual_batchsize):
        if len(self.batches[idx]['reply']) == 0:
            return None

        reply = Batch(self.batches[idx]['reply']).first_k(actual_batchsize)
        return reply

        # if self.batches[idx]['reply'] is not None:
        #     sel_reply = Batch(self.batches[idx]['reply']).first_k(actual_batchsize)

        #     sel_reply.device = self.batches[idx]['device']
        # else:
        #     sel_reply = None
        # return sel_reply

    def _alloc(self, p):
        type_name = p.field().type_name()
        sz = p.field().sz().vec()
        # print('alloc:', p.field().name(), type_name, sz)

        data = self.converter.c2torch(type_name)(*sz).pin_memory()
        data.fill_(1)

        # Return pointer, size and byte_stride
        strides = [i * data.element_size() for i in data.stride()]
        info = elf.PointerInfo()
        info.type = type_name
        info.p = data.data_ptr()
        info.stride = strides
        p.setData(info)

        return data
