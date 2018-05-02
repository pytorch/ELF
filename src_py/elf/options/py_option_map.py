# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import json

import elf
from _elf import _options


class PyOptionMap(_options.OptionMap):
    """Override C++ OptionMap with additional bells and whistles."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def getPyOptionSpec(self):
        return elf.options.PyOptionSpec(super().getOptionSpec())

    def loadOptionDict(self, option_dict):
        return self.loadJSONString(json.dumps(option_dict))

    def getOptionDict(self):
        return json.loads(self.getJSONString())

    def get(self, option_name):
        return json.loads(self.getAsJSONString(option_name))

    def storeIntoNamespace(self, namespace, option_spec=None):
        """Stores the parameters from a PyOptionMap into a namespace."""
        if option_spec is None:
            option_spec = self.getPyOptionSpec()

        option_names = option_spec.getOptionNames()
        for name in option_names:
            setattr(namespace, name, self.get(name))

    def clone(self):
        return PyOptionMap(self)

    def __deepcopy__(self, memo):
        return self.clone()
