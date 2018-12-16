# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch.nn as nn
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec

from .utils import average_norm_clip


class ValueMatcher(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addFloatOption(
            'grad_clip_norm',
            'gradient norm clipping',
            0.0)
        spec.addStrOption(
            'value_node',
            'name of the value node',
            'V')
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialization of value matcher.

        Initialize value loss to be ``nn.SmoothL1Loss``.
        """
        self.value_loss = nn.SmoothL1Loss().cuda()

    def _reg_backward(self, v):
        ''' Register the backward hook. Clip the gradient if necessary.'''
        grad_clip_norm = self.options.grad_clip_norm
        if grad_clip_norm > 1e-20:
            def bw_hook(grad_in):
                grad = grad_in.clone()
                if grad_clip_norm is not None:
                    average_norm_clip(grad, grad_clip_norm)
                return grad

            v.register_hook(bw_hook)

    def feed(self, batch, stats):
        """
        One iteration of value match.

        nabla_w Loss(V - target)

        Keys in a batch:

        ``V`` (variable): value

        ``target`` (tensor): target value.

        Inputs that are of type Variable can backpropagate.

        Feed to stats: predicted value and value error

        Returns:
            value_err
        """
        V = batch[self.options.value_node]
        value_err = self.value_loss(V, Variable(batch["target"]))
        self._reg_backward(V)
        stats["predicted_" + self.options.value_node].feed(V.data[0])
        stats[self.options.value_node + "_err"].feed(value_err.data[0])

        return value_err
