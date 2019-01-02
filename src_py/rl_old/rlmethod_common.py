# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

from elf.options import auto_import_options, PyOptionSpec

import torch
import torch.nn as nn
from torch.autograd import Variable
import math

def average_norm_clip(grad, clip_val):
    '''
    Compute the norm and clip it if necessary.
    The first dimension will be batchsize.
    Args:
        grad(Tensor): the gradient
        clip_val(float): value to clip to
    '''
    batchsize = grad.size(0)
    avg_l2_norm = 0.0
    for i in range(batchsize):
        avg_l2_norm += grad[i].data.norm()
    avg_l2_norm /= batchsize
    if avg_l2_norm > clip_val:
        # print("l2_norm: %.5f clipped to %.5f" % (avg_l2_norm, clip_val))
        grad *= clip_val / avg_l2_norm

# Methods.

# Actor critic model.
class ActorCritic(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addFloatOption(
            'entropy_ratio',
            '',
            0.01)

        spec.addFloatOption(
            'grad_clip_norm',
            '',
            0.0)

        spec.addFloatOption(
            'discount',
            '',
            0.99
        )

        spec.addFloatOption(
            'min_prob',
            '',
            1e-6)

        spec.addFloatOption(
            'adv_clip',
            '',
            0.0)

        spec.addFloatOption(
            'ratio_clamp',
            '',
            10.0)

        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.option_map = option_map

        self.args = self.options
        args = self.options

        self.policy_loss = nn.NLLLoss().cuda()
        self.value_loss = nn.SmoothL1Loss().cuda()

        if args.grad_clip_norm < 1e-8:
            grad_clip_norm = None
        else:
            grad_clip_norm = args.grad_clip_norm

        self.advantage = None

        def _policy_backward(layer, grad_input, grad_output):
            if self.advantage is None: return
            # Multiply gradient weights

            # This works only on pytorch 0.2.0
            grad_input[0].data.mul_(self.advantage.view(-1, 1))
            if grad_clip_norm is not None:
                average_norm_clip(grad_input[0], grad_clip_norm)

        def _value_backward(layer, grad_input, grad_output):
            if grad_clip_norm is not None:
                average_norm_clip(grad_input[0], grad_clip_norm)

        # Backward hook for training.
        self.policy_loss.register_backward_hook(_policy_backward)
        self.value_loss.register_backward_hook(_value_backward)

    def _compute_one_policy_entropy_err(self, pi, a, min_prob):
        batchsize = pi.size(0)

        # Add normalization constant
        logpi = (pi + min_prob).log()

        # Get policy. N * #num_actions
        policy_err = self.policy_loss(logpi, Variable(a))
        entropy_err = (logpi * pi).sum() / batchsize
        return dict(policy_err=policy_err, entropy_err=entropy_err)

    def _compute_policy_entropy_err(self, pi, a):
        args = self.args

        errs = { }
        if isinstance(pi, list):
            # Action map, and we need compute the error one by one.
            for i, pix in enumerate(pi):
                for j, pixy in enumerate(pix):
                    errs = accumulate(errs, self._compute_one_policy_entropy_err(pixy, a[:,i,j], args.min_prob))
        else:
            errs = self._compute_one_policy_entropy_err(pi, a, args.min_prob)

        return errs

    def update(self, m, batch, stats):
        ''' Actor critic model '''
        args = self.args

        T = batch.histSize("a")

        state_curr = m(batch.hist(T - 1))
        R = state_curr["V"].squeeze().data
        batchsize = batch.batchsize

        r = batch.hist(T - 1, key="r").squeeze()
        term = batch.hist(T - 1, key="terminal").squeeze()
        for i, terminal in enumerate(term):
            if terminal:
                R[i] = r[i]

        stats["init_reward"].feed(R.mean())
        stats["reward"].feed(r.mean())

        for t in range(T - 2, -1, -1):
            state_curr = m(batch.hist(t))

            # go through the sample and get the rewards.
            a = batch.hist(t, key="a").squeeze()
            r = batch.hist(t, key="r").squeeze()
            term = batch.hist(t, key="terminal").squeeze()

            # Compute the reward.
            R = R * args.discount + r
            # If we see any terminal signal, break the reward backpropagation chain.
            for i, terminal in enumerate(term):
                if terminal:
                    R[i] = r[i]

            pi = state_curr["pi"]
            old_pi = batch.hist(t, key="pi").squeeze()
            V = state_curr["V"].squeeze()

            # We need to set it beforehand.
            # Note that the samples we collect might be off-policy, so we need
            # to do importance sampling.
            self.advantage = R - V.data
            # truncate adv. 
            if self.options.adv_clip > 1e-5:
                self.advantage.clamp_(min=-self.options.adv_clip, max=self.options.adv_clip)

            # Cap it.
            coeff = torch.clamp(pi.data.div(old_pi), max=self.options.ratio_clamp).gather(1, a.view(-1, 1)).squeeze()
            self.advantage.mul_(coeff)
            # There is another term (to compensate clamping), but we omit it for
            # now.

            # Compute policy gradient error:
            errs = self._compute_policy_entropy_err(pi, a)
            policy_err = errs["policy_err"]
            entropy_err = errs["entropy_err"]

            # Compute critic error
            value_err = self.value_loss(V, Variable(R))

            overall_err = policy_err + entropy_err * args.entropy_ratio + value_err
            overall_err.backward()

            stats["rms_advantage"].feed(self.advantage.norm() / math.sqrt(batchsize))
            stats["cost"].feed(overall_err.item())
            stats["predict_reward"].feed(V.mean())
            stats["reward"].feed(r.mean())
            stats["acc_reward"].feed(R.mean())
            stats["value_err"].feed(value_err.item())
            stats["policy_err"].feed(policy_err.item())
            stats["entropy_err"].feed(entropy_err.item())
            #print("[%d]: reward=%.4f, sum_reward=%.2f, acc_reward=%.4f, value_err=%.4f, policy_err=%.4f" % (i, r.mean(), r.sum(), R.mean(), value_err.data[0], policy_err.data[0]))

