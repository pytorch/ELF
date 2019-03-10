"""utils"""


def assert_eq(real, expected, msg='assert_eq fails'):
    assert real == expected, '%s: %s (real) vs %s (expected)' % (msg, real, expected)
