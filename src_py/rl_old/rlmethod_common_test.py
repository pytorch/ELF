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

    def update(self, m, batch, stats):
        ''' Actor critic model '''
        args = self.args
        bs = batch.batchsize

        T = batch.histSize("a")
        batch["s"] = batch["s"].view(T * bs, *(batch["s"].size()[2:]))
        state_curr = m(batch)

        state_curr["V"] = state_curr["V"].view(bs, T, *(state_curr["V"].size()[1:]))
        state_curr["pi"] = state_curr["pi"].view(bs, T, *(state_curr["pi"].size()[1:]))

        R = state_curr["V"][:, T-1].data.squeeze()
        batchsize = batch.batchsize

        r = batch.hist(T - 1, key="r").squeeze()
        term = batch.hist(T - 1, key="terminal").squeeze()
        for i, terminal in enumerate(term):
            if terminal:
                R[i] = r[i]

        stats["init_reward"].feed(R.mean())
        stats["reward"].feed(r.mean())

        loss = 0

        for t in range(T - 2, -1, -1):
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

            pi = state_curr["pi"][:,t]
            old_pi = batch.hist(t, key="pi").squeeze()
            V = state_curr["V"][:,t].squeeze()

            # We need to set it beforehand.
            # Note that the samples we collect might be off-policy, so we need
            # to do importance sampling.
            advantage = R - V.data
            # truncate adv. 
            if self.options.adv_clip > 1e-5:
                advantage.clamp_(min=-self.options.adv_clip, max=self.options.adv_clip)

            # Cap it.
            coeff = torch.clamp(pi.data.div(old_pi), max=self.options.ratio_clamp).gather(1, a.view(-1, 1)).squeeze()
            advantage.mul_(coeff)
            # There is another term (to compensate clamping), but we omit it for
            # now.

            # Add normalization constant
            logpi = (pi + args.min_prob).log()

            # Get policy. N * #num_actions
            policy_err = -(logpi.index_select(1, a) * advantage).mean()
            entropy_err = (logpi * pi).sum() / bs

            # Compute critic error
            value_err = self.value_loss(V, Variable(R))

            overall_err = policy_err + entropy_err * args.entropy_ratio + value_err
            loss += overall_err

            stats["rms_advantage"].feed(advantage.norm() / math.sqrt(batchsize))
            stats["predict_reward"].feed(V.mean())
            stats["reward"].feed(r.mean())
            stats["acc_reward"].feed(R.mean())
            stats["value_err"].feed(value_err.item())
            stats["policy_err"].feed(policy_err.item())
            stats["entropy_err"].feed(entropy_err.item())
            #print("[%d]: reward=%.4f, sum_reward=%.2f, acc_reward=%.4f, value_err=%.4f, policy_err=%.4f" % (i, r.mean(), r.sum(), R.mean(), value_err.data[0], policy_err.data[0]))

        stats["cost"].feed(loss.item())
        loss.backward()

