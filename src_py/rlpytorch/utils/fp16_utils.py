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


def convert_fp16_if(module, condition):
    """Nonrecursively converts a module's parameters and buffers to fp16
    if a given condition is met.
    """
    if condition(module):
        apply_nonrecursive(
            module, lambda t: t.half() if t.is_floating_point() else t)


class FP16Model(Model):
    def __init__(self, option_map, params, model):
        super().__init__(option_map, params)

        def should_convert_to_fp16(module):
            return not isinstance(
                module, torch.nn.modules.batchnorm._BatchNorm)

        self.fp16_model = convert_fp16_if(
            model.float(), should_convert_to_fp16)

    def forward(self, input):
        fp16_input = input.half()
        return self.fp16_model(fp16_input)
