# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.


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


def accumulate(acc, new):
    ''' accumulate by the same key in a list of dicts

    Args:
        acc(dict): the dict to accumulate to
        new(dict): new dict entry

    Returns:
        A new dict containing the accumulated sums of each key.
    '''
    ret = {k: new[k] if a is None else a + new[k]
           for k, a in acc.items() if k in new}
    ret.update({k: v for k, v in new.items() if not (k in acc)})
    return ret


def add_err(overall_err, new_err):
    ''' Add ``new_err`` to ``overall_err``

    Args:
        overall_err(float): summed overall error
        new_err(float): new error
    '''
    if overall_err is None:
        return new_err
    else:
        overall_err += new_err
        return overall_err


def add_stats(stats, key, value):
    ''' Feed ``value`` to ``stats[key]``'''
    if stats:
        stats[key].feed(value)


def check_terminals(has_terminal, batch):
    ''' Check if the environment sent a terminal signal '''
    # Block backpropagation if we go pass a terminal node.
    for i, terminal in enumerate(batch["terminal"]):
        if terminal:
            has_terminal[i] = True


def check_terminals_anyT(has_terminal, batch, T):
    ''' Check if any of ``batch[t], t <= T`` is terminal'''
    for t in range(T):
        check_terminals(has_terminal, batch[t])
