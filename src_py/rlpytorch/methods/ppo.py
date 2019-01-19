# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

import torch
import torch.nn as nn
import numpy as np
from elf.options import auto_import_options, PyOptionSpec
from .discounted_reward import DiscountedReward
from rlpytorch import utils


class LinearDecaySchedule:
    def __init__(self, init, final, epochs):
        self.init = init
        self.final = final
        self.epochs = epochs
        self.step_size = (final - init) / epochs

    def get_ratio(self, epoch):
        return self.init - self.step_size * epoch


class PPO:
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addIntOption(
            'anneal_entropy',
            '',
            0)
        spec.addFloatOption(
            'entropy_ratio',
            '',
            0.01)
        spec.addFloatOption(
            'init_entropy_ratio',
            'the entropy ratio we put on PolicyGradient',
            0.01)
        spec.addFloatOption(
            'final_entropy_ratio',
            '',
            0.0)
        spec.addIntOption(
            'entropy_decay_epoch',
            'decay the entropy linearly during the first k epochs',
            100)
        spec.addFloatOption(
            'min_prob',
            'mininal probability used in training',
            1e-6)
        spec.addFloatOption(
            'ratio_clamp',
            'maximum importance sampling ratio',
            0.1)
        spec.addFloatOption(
            'max_grad_norm',
            'maximum norm of gradient',
            0.5)
        spec.addFloatOption(
            'discount',
            'exponential discount rate',
            0.99)
        return spec

    @auto_import_options
    def __init__(self, option_map):
        self.max_log_ratio = np.log(self.options.ratio_clamp)
        self.discounted_reward = DiscountedReward(self.options.discount)
        if self.options.anneal_entropy:
            self.entropy_decay = LinearDecaySchedule(
                self.options.init_entropy_ratio,
                self.options.final_entropy_ratio,
                self.options.entropy_decay_epoch)

    def update(self, model, batch, stats, epoch_idx=None):
        """PPO model update.

        Feed stats for later summarization.

        Args:
            mi(`ModelInterface`): mode interface used
            batch(dict): batch of data. Keys in a batch:
                ``s``: state,
                ``r``: immediate reward,
                ``terminal``: if game is terminated
            stats(`Stats`): Feed stats for later summarization.
        """
        T = batch.histSize("a")

        inputs = batch.hist(T - 1, None)
        outputs = model(inputs)
        self.discounted_reward.setR(outputs["V"].squeeze().detach(), stats)

        # self._print_batch(batch, True)

        # err = 0
        for t in range(T - 2, -1, -1):
            # go through the sample and get the rewards.
            inputs = batch.hist(t, None)
            outputs = model(inputs)

            V = outputs["V"].squeeze()
            pi = outputs["pi"].clamp(self.options.min_prob, 1 - self.options.min_prob)
            R = self.discounted_reward.feed(
                inputs["r"],
                inputs["terminal"].float(),
                stats)

            behavior_pi = inputs["pi"]
            action = inputs["a"]

            # errors [batchsize]
            q = R - V
            value_err = 0.5 * q.pow(2)
            policy_err = policy_gradient_error(
                pi,
                q.detach(),
                action,
                behavior_pi,
                lambda probs, a : probs.gather(1, a.view(-1, 1)).squeeze().log(),
                self.max_log_ratio)

            logpi = pi.log()
            entropy = -(pi * logpi).sum(1)

            utils.assert_eq(value_err.dim(), 1)
            utils.assert_eq(value_err.size(), policy_err.size())
            value_err = value_err.mean()
            policy_err = policy_err.mean()
            entropy = entropy.mean()

            stats["err_val"].feed(value_err.detach().item())
            stats["err_pi"].feed(policy_err.detach().item())
            stats["entropy"].feed(entropy.detach().item())

            if self.options.anneal_entropy:
                entropy_ratio = self.entropy_decay.get_ratio(epoch_idx)
            else:
                entropy_ratio = self.options.entropy_ratio

            # err += (value_err + policy_err - entropy * entropy_ratio)
            err = value_err + policy_err - entropy * entropy_ratio
            err.backward()
            stats["cost"].feed(err.detach().item())

        # err.backward()
        grad_norm = nn.utils.clip_grad_norm_(
            model.parameters(), self.options.max_grad_norm)
        stats["grad_norm"].feed(grad_norm)


def policy_gradient_error(pi, q, action, behavior_pi, log_prob_func, ratio_clamp):
    """Compute policy gradient error of policy

    params:
        pi: [batch_size, num_action] policy being evaluated
        q: [batch_size] estimate of state-action value
        action: [batch_size, num_action] action taken
        behavior_pi: [batch_size, num_action]
            behavior policy used during experience collection None if on-policy
    """
    # utils.assert_eq(action.size(), pi.size())
    # utils.assert_eq(q.size(), (pi.size(0),))

    # if behavior_pi is not None:
    #     pass
    #     # utils.assert_eq(behavior_pi.size(), pi.size())
    action_logp = log_prob_func(pi, action)

    assert behavior_pi is not None

    behavior_action_logp = log_prob_func(behavior_pi, action)
    log_ratio = action_logp - behavior_action_logp
    # log_ratio = log_ratio.clamp(max=max_log_ratio)
    ratio = log_ratio.exp()
    clamped_ratio = ratio.clamp(min=1-ratio_clamp, max=1+ratio_clamp)
    err1 = q * ratio
    err2 = q * clamped_ratio
    err = -torch.min(err1, err2)
    return err
