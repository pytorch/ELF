# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import torch
import torch.nn as nn
from torch.autograd import Variable

from elf.options import auto_import_options, PyOptionSpec
from .utils import accumulate, add_err, average_norm_clip


class PolicyGradient(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addFloatOption(
            'entropy_ratio',
            'the entropy ratio we put on PolicyGradient',
            0.01)
        spec.addFloatOption(
            'grad_clip_norm',
            'gradient norm clipping',
            0.0)
        spec.addFloatOption(
            'min_prob',
            'mininal probability used in training',
            1e-6)
        spec.addFloatOption(
            'ratio_clamp',
            'maximum importance sampling ratio',
            10.0)
        spec.addStrListOption(
            'policy_action_nodes',
            'the entropy ratio we put on PolicyGradient',
            ['pi,a'])
        return spec

    @auto_import_options
    def __init__(self, option_map):
        """Initialize the policy gradient.

        Specifically, initialize policy loss to be an ``nn.NLLLoss`` and
        parse ``policy_action_nodes``.
        """
        self.policy_loss = nn.NLLLoss().cuda()
        self.policy_action_nodes = []
        for node_specifier in self.options.policy_action_nodes:
            policy, _, action = node_specifier.partition(",")
            self.policy_action_nodes.append((policy, action))

    def _compute_one_policy_entropy_err(self, pi, a):
        """Compute policy error and entropy error for one.

        Pass in ``min_prob`` to avoid ``Nan`` in logrithms.

        Returns:
            dict of
            ``logpi``: log policy
            ``policy_err``: polict error
            ``entropy_err``: entropy error
        """
        batchsize = a.size(0)

        # Add normalization constant
        logpi = (pi + self.options.min_prob).log()
        # TODO Seems that logpi.clone() won't create a few hook list.
        # See https://github.com/pytorch/pytorch/issues/2601
        logpi2 = (pi + self.options.min_prob).log()

        # Get policy. N * #num_actions
        policy_err = self.policy_loss(logpi, a)
        entropy_err = (logpi2 * pi).sum() / batchsize
        return dict(
            logpi=logpi,
            policy_err=policy_err,
            entropy_err=entropy_err)

    def _compute_policy_entropy_err(self, pi, a):
        """Compute policy error and entropy error for a batch.

        Pass in ``min_prob`` to avoid ``Nan`` in logrithms.

        Returns:
            dict of
            ``logpi``: log policy
            ``policy_err``: polict error
            ``entropy_err``: entropy error
        """
        errs = {}
        if isinstance(pi, list):
            # Action map, and we need compute the error one by one.
            for i, pix in enumerate(pi):
                for j, pixy in enumerate(pix):
                    errs = accumulate(
                        errs,
                        self._compute_one_policy_entropy_err(
                            pixy, a[:, i, j], self.options.min_prob)
                    )
        else:
            errs = self._compute_one_policy_entropy_err(pi, a)

        return errs

    def _reg_backward(self, v, pg_weights):
        """Register the backward hook. Clip the gradient if necessary."""
        def bw_hook(grad_in):
            grad_clip_norm = self.options.grad_clip_norm
            # this works only on pytorch 0.2.0
            grad = grad_in.mul(pg_weights.view(-1, 1))
            # import pdb
            # pdb.set_trace()
            if grad_clip_norm > 1e-20:
                average_norm_clip(grad, grad_clip_norm)
            return grad
        v.register_hook(bw_hook)

    def feed(self, Q, pi_s, actions, stats, old_pi_s=dict()):
        """One iteration of policy gradient.

        rho nabla_w log p_w(a|s) Q + entropy_ratio * nabla H(pi(.|s))

        Args:
            Q(tensor): estimated return
            actions(tensor): action
            pi_s(variable): policy
            old_pi_s(tensor, optional): old policy, in order to
                                        get importance factor.

        If you specify multiple policies, then all the log prob of these
        policies are added, and their importance factors are multiplied.


        Feed to stats: policy error and nll error
        """
        # We need to set it beforehand.
        # Note that the samples we collect might be off-policy, so we need
        # to do importance sampling.
        pg_weights = Q.clone()

        policy_err = None
        entropy_err = None
        log_pi_s = []

        for pi_node, a_node in self.policy_action_nodes:
            pi = pi_s[pi_node]
            a = actions[a_node].squeeze()

            if pi_node in old_pi_s:
                old_pi = old_pi_s[pi_node].squeeze()

                # Cap it.
                clamped_ratios = torch.clamp(
                    pi.data.div(old_pi), max=self.options.ratio_clamp)
                coeff = clamped_ratios.gather(1, a.view(-1, 1)).squeeze()
                pg_weights.mul_(coeff)
                # There is another term (to compensate clamping), but we omit
                # it for now.

            # Compute policy gradient error:
            errs = self._compute_policy_entropy_err(pi, Variable(a))
            policy_err = add_err(policy_err, errs["policy_err"])
            entropy_err = add_err(entropy_err, errs["entropy_err"])
            log_pi_s.append(errs["logpi"])

            stats["nll_" + pi_node].feed(errs["policy_err"].data[0])
            stats["entropy_" + pi_node].feed(errs["entropy_err"].data[0])

        for log_pi in log_pi_s:
            self._reg_backward(log_pi, Variable(pg_weights))

        if len(self.policy_action_nodes) > 1:
            stats["total_nll"].feed(policy_err.data[0])
            stats["total_entropy"].feed(entropy_err.data[0])

        return policy_err + entropy_err * self.options.entropy_ratio
