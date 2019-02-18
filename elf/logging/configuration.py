# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

from elf.options import auto_import_options, PyOptionSpec

from . import LoggerLevel, set_level


class GlobalLoggingConfigurator(object):
    """Global configurator for logging."""
    @classmethod
    def get_option_spec(cls):
        spec = PyOptionSpec()
        spec.addStrOption(
            'loglevel',
            ('Global log level. Choose from '
             'trace, debug, info, warning, error, critical, or off)'),
            'info')
        return spec

    @auto_import_options
    def __init__(self, option_map):
        pass

    def configure(self):
        loglevel = LoggerLevel.from_str(self.options.loglevel)
        assert loglevel != LoggerLevel.invalid
        set_level(loglevel)
