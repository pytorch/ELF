# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.
from ..utils import assert_eq

def tensor_slice(t, dim, b, e):
    if dim == 0:
        return t[b:e]
    elif dim == 1:
        return t[:, b:e]
    elif dim == 2:
        return t[:, :, b:e]
    else:
        raise ValueError('unsupported %d in tensor_slice' % dim)


def tensor_sel(t, dim, b):
    return tensor_slice(t, dim, b, b + 1).squeeze(dim)


class Batch:
    def __init__(self, batch):
        self.batch = batch

    def __getitem__(self, key):
        return self.batch[key]

    def __setitem__(self, key, v):
        self.batch[key] = v

    def __contains__(self, key):
        return key in self.batch

    def keys(self):
        return self.batch.keys()

    def first_k(self, batchsize):
        new_batch = {
            k: tensor_slice(v, 0, 0, batchsize) for k,
            v in self.batch.items()
        }
        return Batch(new_batch)

    def to(self, device):
        new_batch = {
            k: v.to(device, non_blocking=True) for k, v in self.batch.items()
        }
        return Batch(new_batch)

    def index_with_dim(self, idx, dim, key=None):
        assert idx >= 0

        if key is None:
            new_batch = {
                k: tensor_sel(v, dim, idx)
                for k, v in self.batch.items()
            }

        if isinstance(key, str):
            key = [key]

        if isinstance(key, list):
            new_batch = {
                k: tensor_sel(self.batch[k], dim, idx)
                for k in key
            }

        return Batch(new_batch)

    def copy_(self, src):
        """copy all keys and values from another dict or `Batch` object

        Args:
            src(dict or `Batch`): batch data to be copied
        """
        if isinstance(src, Batch):
            src = src.batch

        missing_keys = [k for k in self.batch if k not in src]
        extra_keys = []

        for key, src_data in src.items():
            if key not in self.batch:
                extra_keys.append(key)
                continue

            dest = self.batch[key]
            assert_eq(src_data.size(),
                      dest.size(),
                      'Batch.copy_(): size mismatch for key = %s' % key)
            assert_eq(src_data.dtype,
                      dest.dtype,
                      'Batch.copy_(): dtype mismatch for key = %s' % key)
            dest.copy_(src_data)

        return extra_keys, missing_keys
