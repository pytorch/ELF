# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import argparse
import json
import sys

import elf
from _elf import _options


# We can get rid of this and just eval() the type name.
# Depends on how safe we want to be.
_typename_to_type = {
    'str': str,
    'int': int,
    'float': float,
    'bool': bool,
}


class PyOptionSpec(_options.OptionSpec):
    """Override C++ OptionSpec with additional bells and whistles."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def getArgparseOptions(self):
        return json.loads(self.getPythonArgparseOptionsAsJSONString())

    def toArgparser(self):
        """Creates an ArgumentParser from a PyOptionSpec."""
        parser = argparse.ArgumentParser()
        parser_options = self.getArgparseOptions()

        for parser_option in parser_options:
            if 'type' in parser_option['kwargs']:
                parser_option['kwargs']['type'] = \
                    _typename_to_type[parser_option['kwargs']['type']]
            parser.add_argument(
                *parser_option["args"],
                **parser_option["kwargs"])

        return parser

    def parse(self, args=None, overrides=None):
        """Given a PyOptionSpec, parses the command line parameters
        (``sys.argv```) and returns the resulting PyOptionMap.

        ``args`` can override ``sys.argv`` and ``overrides`` can override
        any parsed items.
        """

        parser = self.toArgparser()
        arg_namespace = parser.parse_args(args=args)

        if overrides:
            for k, v in overrides.items():
                setattr(arg_namespace, k, v)

        arg_namespace.parsed_args = list(sys.argv if args is None else args)

        option_map = elf.options.PyOptionMap(self)
        option_map.loadOptionDict(vars(arg_namespace))
        return option_map

    @classmethod
    def fromClasses(cls, classes):
        option_spec = cls()
        for c in classes:
            option_spec.merge(c.get_option_spec())
        return option_spec

    def clone(self):
        return PyOptionSpec(self)

    def __deepcopy__(self, memo):
        return self.clone()
