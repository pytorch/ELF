# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import argparse


def import_options(obj, option_map, option_spec, namespace=None):
    """Stores the parameters from a PyOptionMap into ``obj.options``."""
    if namespace is None:
        setattr(obj, 'options', argparse.Namespace())
        namespace = obj.options

    if option_spec is None:
        option_spec = option_map.getPyOptionSpec()

    option_map.storeIntoNamespace(namespace, option_spec)


def auto_import_options(fn):
    """This decorator applies to __init__ methods where the first argument
    is a PyOptionMap.

    It copies each required argument (as specified by the class's
    ``get_option_spec()``) from the PyOptionMap into the object namespace
    of ``self.options`` (i.e. ``self.options.blah``).
    """

    def call(self, option_map, *args, **kwargs):
        import_options(self, option_map, self.get_option_spec())
        return fn(self, option_map, *args, **kwargs)

    return call
