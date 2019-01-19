"""Utils for assertions"""


def assert_eq(real, expected):
    assert real == expected, '%s (true) vs %s (expected)' % (real, expected)


def assert_neq(real, expected):
    assert real != expected, '%s (true) vs %s (expected)' % (real, expected)


def assert_lt(real, expected):
    assert real < expected, '%s (true) vs %s (expected)' % (real, expected)


def assert_lteq(real, expected):
    assert real <= expected, '%s (true) vs %s (expected)' % (real, expected)


def assert_tensor_eq(t1, t2, eps=1e-6):
    if t1.size() != t2.size():
        print('Warning: size mismatch', t1.size(), 'vs', t2.size())
        return False

    t1 = t1.cpu().numpy()
    t2 = t2.cpu().numpy()
    diff = abs(t1 - t2)
    eq = (diff < eps).all()
    if not eq:
        import pdb
        pdb.set_trace()
    assert(eq)


def assert_zero_grads(params):
    for p in params:
        if p.grad is not None:
            assert(p.grad.sum().item() == 0)
