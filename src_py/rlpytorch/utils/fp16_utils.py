# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch
from rlpytorch import Model


def apply_nonrecursive(module, fn):
    """Applies a given function only to parameters and buffers of a module.

    Adapted from torch.nn.Module._apply.
    """
    for param in module._parameters.values():
        if param is not None:
            # Tensors stored in modules are graph leaves, and we don't
            # want to create copy nodes, so we have to unpack the data.
            param.data = fn(param.data)
            if param._grad is not None:
                param._grad.data = fn(param._grad.data)

    for key, buf in module._buffers.items():
        if buf is not None:
            module._buffers[key] = fn(buf)

    return module


class FP16Model(Model):
    def __init__(self, option_map, params, model):
        super().__init__(option_map, params)

        self.model = model.float()
        for module in model.modules():
            if not isinstance(module, torch.nn.modules.batchnorm._BatchNorm):
                apply_nonrecursive(
                    module, lambda t: t.half() if t.is_floating_point() else t)

    def forward(self, input):
        fp16_input = input.half()
        return self.model(fp16_input)
