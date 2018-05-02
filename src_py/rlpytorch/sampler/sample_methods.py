# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import numpy as np
import torch
import sys


def uniform_multinomial(batchsize, num_action, use_cuda=True):
    ''' Sample with uniform probability.

    Args:
        batchsize(int): batch size
        num_action(int): total number of actions to sample
        use_cuda(bool): indicates if tensor is put on cuda
    '''
    # [TODO] Make the type more friendly
    if use_cuda:
        uniform_p = torch.cuda.FloatTensor(num_action).fill_(1.0 / num_action)
    else:
        uniform_p = torch.FloatTensor(num_action).fill_(1.0 / num_action)

    return uniform_p.multinomial(batchsize, replacement=True)


def sample_with_check(probs, greedy=False):
    """multinomial sampling with out of bound check

    Args:
        probs(tensor): probability to sample from
        greedy(bool): if ``True``, pick the action with maximum probability,
        otherwise sample from it.
    """
    num_action = probs.size(1)
    if greedy:
        _, actions = probs.max(1)
        return actions
    while True:
        actions = probs.multinomial(1)[:, 0]
        cond1 = (actions < 0).sum()
        cond2 = (actions >= num_action).sum()
        if cond1 == 0 and cond2 == 0:
            return actions
        print(
            "Warning! sampling out of bound! cond1 = %d, cond2 = %d" %
            (cond1, cond2))
        print("prob = ")
        print(probs)
        print("action = ")
        print(actions)
        print("condition1 = ")
        print(actions < 0)
        print("condition2 = ")
        print(actions >= num_action)
        print("#actions = ")
        print(num_action)
        sys.stdout.flush()


def sample_eps_with_check(probs, epsilon, greedy=False):
    """multinomial sampling with out of bound check,
    with at least ``epsilon`` probability

    Args:
        probs(tensor): probability to sample from
        epsilon(float): Minimum probability in sampling
        greedy(bool): if ``True``, pick the action with maximum probability,
                      otherwise sample from it.
    """
    # actions = self.sample_policy(state_curr[self.sample_node].data, args)
    actions = sample_with_check(probs, greedy=greedy)

    if epsilon > 1e-10:
        num_action = probs.size(1)
        batchsize = probs.size(0)

        probs = probs.data if isinstance(
            probs, torch.autograd.Variable) else probs

        rej_p = probs.new().resize_(2)
        rej_p[0] = 1 - epsilon
        rej_p[1] = epsilon
        rej = rej_p.multinomial(batchsize, replacement=True).byte()

        uniform_p = probs.new().resize_(num_action).fill_(1.0 / num_action)
        uniform_sampling = uniform_p.multinomial(batchsize, replacement=True)
        actions[rej] = uniform_sampling[rej]
    return actions


def sample_multinomial(state_curr, args, node="pi", greedy=False):
    ''' multinomial sampling

    Args:
        state_curr(dict): current state containing all data
        args(dict): customized arguments for sampling. `epsilon` is used
        node(str): name string for policy, default is "pi"
        greedy(bool): if ``True``, pick the action with maximum probability,
                      otherwise sample from it.

    Returns:
        A list of actions using multinomial sampling.
    '''
    if isinstance(state_curr[node], list):
        # Action map
        probs = state_curr[node]
        rx = len(probs)
        ry = len(probs[0])
        batchsize = probs[0][0].size(0)

        actions = [np.zeros((rx, ry), dtype='int32') for i in range(batchsize)]

        for i, actionx_prob in enumerate(probs):
            for j, action_prob in enumerate(actionx_prob):
                this_action = sample_eps_with_check(
                    action_prob.data, args.epsilon, greedy=greedy)
                for k in range(batchsize):
                    actions[k][i, j] = this_action[k]
        return actions
    else:
        probs = state_curr[node].data
        return sample_eps_with_check(probs, args.epsilon, greedy=greedy)


def epsilon_greedy(state_curr, args, node="pi"):
    ''' epsilon greedy sampling

    Args:
        state_curr(dict): current state containing all data
        args(dict): customized arguments for sampling. `epsilon` is used
        node(str): name string for policy, default is "pi"

    Returns:
        A list of actions using epsilon greedy sampling.
    '''
    return sample_multinomial(state_curr, args, node=node, greedy=True)


def original_distribution(state_curr, args, node="pi"):
    ''' Send original probability as it is.

    Args:
        state_curr(dict): current state containing all data
        args(dict): customized arguments for sampling. `epsilon` is used
        node(str): name string for policy, default is "pi"

    Returns:
        A list of original probabilities.
    '''
    probs = state_curr[node].data
    batchsize = probs.size(0)
    # Return a list of list.
    return [list(probs[i]) for i in range(batchsize)]
