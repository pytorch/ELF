# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import sys
from collections import defaultdict

import numpy as np
import torch


class Allocator(object):
    ''' A wrapper class for batch data'''
    torch_types = {
        "int32_t": torch.IntTensor,
        "int64_t": torch.LongTensor,
        "float": torch.FloatTensor,
        "unsigned char": torch.ByteTensor,
        "char": torch.ByteTensor
    }
    numpy_types = {
        "int32_t": 'i4',
        'int64_t': 'i8',
        'float': 'f4',
        'unsigned char': 'byte',
        'char': 'byte'
    }

    @staticmethod
    def _alloc(p, gpu, use_numpy=True):
        name = p.field().name()
        type_name = p.field().type_name()
        sz = p.field().sz().vec()

        print(name, type_name, sz)

        if not use_numpy:
            v = Allocator.torch_types[type_name](*sz)
            if gpu is not None:
                with torch.cuda.device(gpu):
                    v = v.pin_memory()
            v.fill_(1)
            # Return pointer, size and byte_stride
            strides = [i * v.element_size() for i in v.stride()]
            p.set(v.data_ptr(), strides)
        else:
            v = np.zeros(sz, dtype=Allocator.numpy_types[type_name])
            v[:] = 1

            import pdb
            pdb.set_trace()
            # Return pointer, size and byte_size
            p.set(v.ctypes.data, v.strides)

        return name, v

    @staticmethod
    def spec2batches(ctx, batchsize, spec, gpu, use_numpy=False, num_recv=1):
        batch_spec = []
        name2idx = defaultdict(lambda: list())
        idx2name = dict()

        for name, v in spec.items():
            print("%s: %s" % (name, v))
            # TODO this might not good since it changes the input.
            if "input" not in v or v["input"] is None:
                v["input"] = []

            if "reply" not in v or v["reply"] is None:
                v["reply"] = []

            this_batchsize = v.get("batchsize", batchsize)

            keys = list(set(v["input"] + v["reply"]))
            print("SharedMem: \"%s\", keys: %s" % (name, str(keys)))

            smem_opts = ctx.createSharedMemOptions(name, this_batchsize)
            smem_opts.setTimeout(v.get("timeout_usec", 0))

            for _ in range(num_recv):
                smem = ctx.allocateSharedMem(smem_opts, keys)
                spec = dict((
                    Allocator._alloc(smem[field], gpu, use_numpy=use_numpy)
                    for field in keys
                ))

                # Split spec.
                spec_input = {key: spec[key] for key in v["input"]}
                spec_reply = {key: spec[key] for key in v["reply"]}

                batch_spec.append(dict(input=spec_input, reply=spec_reply))

                idx = smem.getSharedMemOptions().idx()
                name2idx[name].append(idx)
                idx2name[idx] = name

        return batch_spec, name2idx, idx2name


def tensor_slice(t, dim, b, e=None):
    if e is None:
        e = b + 1
    if dim == 0:
        return t[b:e]
    elif dim == 1:
        return t[:, b:e]
    elif dim == 2:
        return t[:, :, b:e]
    else:
        raise ValueError("unsupported %d in tensor_slice" % dim)


class Batch:
    def __init__(self, _GC=None, _batchdim=0, _histdim=None, **kwargs):
        '''Initialize `Batch` class.

        Pass in a dict and wrap it into ``self.batch``
        '''
        self.GC = _GC
        self.batchdim = _batchdim
        self.histdim = _histdim
        self.batch = kwargs

    def empty_copy(self):
        batch = Batch()
        batch.GC = self.GC
        batch.batchdim = self.batchdim
        batch.histdim = self.histdim
        return batch

    def first_k(self, batchsize):
        batch = self.empty_copy()
        batch.batch = {
            k: tensor_slice(
                v,
                self.batchdim,
                0,
                batchsize) for k,
            v in self.batch.items()}
        return batch

    def __getitem__(self, key):
        '''Get a key from batch. Can be either ``key`` or ``last_key``

        Args:
            key(str): key name. e.g. if ``r`` is passed in,
                      will search for ``r`` or ``last_r``
        '''
        if key in self.batch:
            return self.batch[key]
        else:
            key_with_last = "last_" + key
            if key_with_last in self.batch:
                return self.batch[key_with_last][1:]
            else:
                raise KeyError(
                    "Batch(): specified key: %s or %s not found!" %
                    (key, key_with_last))

    def add(self, key, value):
        '''Add key=value in Batch.

        This is used when you want to send additional state to the
        learning algorithm, e.g., hidden state collected from the
        previous iterations.
        '''
        self.batch[key] = value
        return self

    def __contains__(self, key):
        return key in self.batch or "last_" + key in self.batch

    def setzero(self):
        ''' Set all tensors in the batch to 0 '''
        for _, v in self.batch.items():
            v[:] = 0

    def copy_from(self, src):
        ''' copy all keys and values from another dict or `Batch` object

        Args:
            src(dict or `Batch`): batch data to be copied
        '''
        this_src = src if isinstance(src, dict) else src.batch
        key_assigned = {k: False for k in self.batch.keys()}

        keys_extra = []

        for k, v in this_src.items():
            print(k)
            # Copy it down to cpu.
            if k not in self.batch:
                keys_extra.append(k)
                continue

            bk = self.batch[k]
            key_assigned[k] = True
            if v is None:
                continue
            if isinstance(v, list) and bk.numel() == len(v):
                bk = bk.view(-1)
                for i, vv in enumerate(v):
                    bk[i] = vv
            elif isinstance(v, (int, float)):
                bk.fill_(v)
            else:
                try:
                    print ("bk:")
                    print (bk)
                    print ("v.squeeze :")
                    print (v.squeeze_())
                    bk[:] = v.squeeze_()
                except BaseException:
                    import pdb
                    pdb.set_trace()

        # Check whether there is any key missing.
        keys_missing = [
            k for k, assigned in key_assigned.items() if not assigned]
        return keys_extra, keys_missing

    def hist(self, hist_idx, key=None):
        '''
        return batch history.

        Args:
            s(int): s=1 means going back in time by one step, etc
            key(str): if None, return all key's history,
                      otherwise just return that key's history
        '''
        if self.histdim is None:
            raise ValueError("No histdim information for the batch")

        if key is None:
            new_batch = self.empty_copy()
            new_batch.batch = {
                k: tensor_slice(v, self.histdim, hist_idx)
                for k, v in self.batch.items()
            }
            return new_batch
        else:
            return tensor_slice(self[key], self.histdim, hist_idx)

    def half(self):
        '''transfer batch data to fp16'''
        new_batch = self.empty_copy()
        new_batch.batch = {k: v.half()
                           for k, v in self.batch.items()}
        return new_batch

    def cpu2gpu(self, gpu, non_blocking=True):
        ''' transfer batch data to gpu '''
        # For each time step
        new_batch = self.empty_copy()
        new_batch.batch = {k: v.cuda(gpu, non_blocking=non_blocking)
                           for k, v in self.batch.items()}
        return new_batch

    def cpu2cpu(self, gpu, non_blocking=True):
        ''' transfer batch data to gpu '''
        # For each time step
        new_batch = self.empty_copy()
        new_batch.batch = {k: v.clone() for k, v in self.batch.items()}
        return new_batch

    def transfer_cpu2gpu(self, batch_gpu, non_blocking=True):
        ''' transfer batch data to gpu '''
        # For each time step
        for k, v in self.batch.items():
            batch_gpu[k].copy_(v, non_blocking=non_blocking)

    def transfer_cpu2cpu(self, batch_dst, non_blocking=True):
        ''' transfer batch data to cpu '''

        # For each time step
        for k, v in self.batch.items():
            batch_dst[k].copy_(v)

    def pin_clone(self):
        ''' clone and pin memory for faster transportations to gpu '''
        batch = self.empty_copy()
        batch.batch = {k: v.clone().pin_memory()
                       for k, v in self.batch.items()}
        return batch

    def to_numpy(self):
        ''' convert batch data to numpy format '''
        return {
            k: (v.numpy() if not isinstance(v, np.ndarray) else v)
            for k, v in self.batch.items()
        }


class GCWrapper:
    def __init__(
            self,
            GC,
            batchsize,
            spec,
            batchdim=0,
            histdim=None,
            use_numpy=False,
            gpu=None,
            params=dict(),
            verbose=True,
            num_recv=1):
        '''Initialize GCWarpper

        Parameters:
            GC(C++ class): Game Context
            co(C type): context parameters.
            descriptions(list of tuple of dict):
              descriptions of input and reply entries.
              Detailed explanation can be seen in
              :doc:`wrapper-python`.
              The Python interface of wrapper.
            use_numpy(boolean): whether we use numpy array (or PyTorch tensors)
            gpu(int): gpu to use.
            params(dict): additional parameters
        '''

        # TODO Make a unified argument server and remove ``params``
        self.batches, self.name2idx, self.idx2name = Allocator.spec2batches(
            GC.ctx(), batchsize, spec,
            use_numpy=use_numpy, gpu=gpu, num_recv=num_recv)
        self.batchdim = batchdim
        self.histdim = histdim
        self.gpu = gpu
        self.params = params
        self.GC = GC
        self._cb = {}

    def reg_has_callback(self, key):
        return key in self.name2idx

    def reg_callback_if_exists(self, key, cb):
        if self.reg_has_callback(key):
            self.reg_callback(key, cb)
            return True
        else:
            return False

    def reg_callback(self, key, cb):
        '''Set callback function for key

        Parameters:
            key(str): the key used to register the callback function.
              If the key is not present in the descriptions,
              return ``False``.
            cb(function): the callback function to be called.
              The callback function has the signature
              ``cb(input_batch, input_batch_gpu, reply_batch)``.
        '''
        if key not in self.name2idx:
            raise ValueError("Callback[%s] is not in the specification" % key)
        if cb is None:
            print("Warning: Callback[%s] is registered to None" % key)

        for idx in self.name2idx[key]:
            # print("Register " + str(cb) + " at idx: %d" % idx)
            self._cb[idx] = cb
        return True

    def _makebatch(self, key_array):
        return Batch(
            _GC=self.GC,
            _batchdim=self.batchdim,
            _histdim=self.histdim,
            **key_array)

    def _call(self, smem, *args, **kwargs):
        idx = smem.getSharedMemOptions().idx()
        # print("smem idx: %d, label: %s" % (idx, self.idx2name[idx]))
        # print(self.name2idx)
        if idx not in self._cb:
            raise ValueError("smem.idx[%d] is not in callback functions" % idx)

        if self._cb[idx] is None:
            return

        batchsize = smem.effective_batchsize()
        assert batchsize > 0

        picked = self._makebatch(self.batches[idx]["input"]).first_k(batchsize)
        if self.gpu is not None:
            picked = picked.cpu2gpu(self.gpu)

        # Save the infos structure, if people want to have access to state
        # directly, they can use infos.s[i], which is a state pointer.
        picked.smem = smem
        picked.batchsize = batchsize
        picked.max_batchsize = smem.getSharedMemOptions().batchsize()

        # Get the reply array
        if self.batches[idx]["reply"] is not None:
            sel_reply = self._makebatch(
                self.batches[idx]["reply"]).first_k(batchsize)
        else:
            sel_reply = None

        reply = self._cb[idx](picked, *args, **kwargs)
        # If reply is meaningful, send them back.
        if isinstance(reply, dict) and sel_reply is not None:
            if self.gpu is not None:
                print ("self.gpu = ", self.gpu)
                with torch.cuda.device(self.gpu):
                    keys_extra, keys_missing = sel_reply.copy_from(reply)
            else:
                keys_extra, keys_missing = sel_reply.copy_from(reply)

            if len(keys_extra) > 0:
                raise ValueError(
                    "Receive extra keys %s from reply!" %
                    str(keys_extra))
            if len(keys_missing) > 0:
                raise ValueError(
                    "Missing keys %s absent in reply!" %
                    str(keys_missing))

    def _check_callbacks(self):
        # Check whether all callbacks are assigned properly.
        for key, indices in self.name2idx.items():
            for idx in indices:
                if idx not in self._cb:
                    raise ValueError(
                        ("GCWrapper.start(): No callback function "
                         "for key = %s and idx = %d") %
                        (key, idx))

    def run(self, *args, **kwargs):
        '''Wait group of an arbitrary collector key.

        Samples in a returned batch are always from the same group,
        but the group key of the batch may be arbitrary.
        '''
        # print("before wait")
        smem = self.GC.ctx().wait()
        # print("before calling")
        self._call(smem, *args, **kwargs)
        # print("before_step")
        self.GC.ctx().step()

    def start(self):
        '''Start all game environments'''
        self._check_callbacks()
        self.GC.ctx().start()

    def stop(self):
        '''Stop all game environments.

        :func:`start()` cannot be called again after :func:`stop()`
        has been called.
        '''
        self.GC.ctx().stop()

    def reg_sig_int(self):
        import signal

        def signal_handler(s, frame):
            print('Detected Ctrl-C!')
            self.stop()
            sys.exit(0)
        signal.signal(signal.SIGINT, signal_handler)
