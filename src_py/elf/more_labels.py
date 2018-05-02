# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import auto_import_options, PyOptionSpec


class MoreLabels(object):
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrListOption(
            'additional_labels',
            'add additional labels in the batch; e.g. id, seq, last_terminal',
            [])
        return spec

    @auto_import_options
    def __init__(self, option_map):
        pass

    def add_labels(self, desc):
        if self.options.additional_labels:
            for _, v in desc.items():
                v["input"].extend(self.options.additional_labels)
