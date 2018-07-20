# Copyright (c) 2018-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

import importlib
import pprint
import random
import time
import torch
import warnings

from elf.options import import_options, PyOptionSpec
from elf import logging
from .model_interface import ModelInterface
from .sampler import Sampler
from .utils.fp16_utils import FP16Model


_logger_factory = logging.IndexedLoggerFactory(
    lambda name: logging.stderr_color_mt(name))


def load_module(mod):
    """Load a python module."""
    module = importlib.import_module(mod)
    print(module, mod)
    return module


class ModelLoader(object):
    """Class to load a previously saved model."""
    @classmethod
    def get_option_spec(cls, model_class=None, model_idx=None):
        spec = PyOptionSpec()
        spec.addStrOption(
            'load',
            'load model',
            '')
        spec.addStrListOption(
            'onload',
            ('functions to call after loading. e.g., reset,zero_first_layer. '
             'These functions are specified in the model'),
            [])
        spec.addStrListOption(
            'omit_keys',
            'omitted keys when loading',
            [])
        spec.addStrListOption(
            'replace_prefix',
            'replace prefix',
            [])
        spec.addIntOption(
            'gpu',
            'which GPU to use',
            -1)
        spec.addBoolOption(
            'check_loaded_options',
            'Toggles consistency check of loaded vs. current model options.',
            True)
        spec.addBoolOption(
            'use_fp16',
            'use_fp16',
            False)
        spec.addFloatOption(
            'load_model_sleep_interval',
            ('If zero, has no effect. If positive, then before loading the '
             'model, we will sleep for an interval of '
             'duration (secs) ~ Uniform[0, load_model_sleep_interval]'),
            0.0)

        if model_class is not None and hasattr(model_class, 'get_option_spec'):
            spec.merge(model_class.get_option_spec())

        idx_suffix = '' if model_idx is None else str(model_idx)
        spec.addPrefixSuffixToOptionNames('', idx_suffix)

        return spec

    def __init__(self, option_map, model_class, model_idx=None, logger=None):
        """Initialize ModelLoader.

        Loading will fail if extra keys are not put in ``omit_keys``
        Args:
            model_class(class): class name of the model
            model_idx(int): index of the model to be loaded.
                            There may be multiple models in an
                            `ModelInterface` to load.
        """
        import_options(
            self, option_map, self.get_option_spec(model_class, model_idx))

        if logger is not None:
            self.logger = logger
        else:
            self.logger = _logger_factory.makeLogger(
                'rlpytorch.model_loader.ModelLoader-',
                f'-model_index{model_idx}')

        self.option_map_for_model = option_map.clone()
        self.model_class = model_class
        self.model_idx = model_idx
        self._on_get_args = lambda *args, **kwargs: None

        option_spec = self.get_option_spec(model_class, model_idx)
        option_names = set(option_spec.getOptionNames())
        model_option_spec = model_class.get_option_spec()
        model_option_names = set(model_option_spec.getOptionNames())

        # Here, the names in option_names are still possibly suffixed with
        # the model_idx. If so, we need to remove this suffix.
        model_options_to_load = {}
        for option_name in option_names:
            if model_idx is not None and option_name.endswith(str(model_idx)):
                # This is the name without the model_idx suffix
                orig_option_name = option_name[:-len(str(model_idx))]
                value = getattr(self.options, option_name)

                setattr(self.options, orig_option_name, value)
                delattr(self.options, option_name)

                if orig_option_name in model_option_names:
                    model_options_to_load[orig_option_name] = value

        if model_options_to_load:
            self.option_map_for_model.loadOptionDict(
                model_options_to_load)

    def load_model(self, params):
        """Actually loads the model with initialized args.

        Call onload funtions if needed.

        Args:
            params(dict): additinoal parameters to be put into args.
        """
        if self.options.load_model_sleep_interval > 1e-7:
            interval = random.random() * self.options.load_model_sleep_interval
            self.logger.info(f'Sleeping for {interval} seconds')
            time.sleep(interval + 1e-7)

        # Initialize models.
        model = self.model_class(self.option_map_for_model, params)

        if self.options.load:
            self.logger.info(f'Loading model from {self.options.load}')
            if self.options.omit_keys:
                self.logger.info(f'Omitting keys {self.options.omit_keys}')

            if self.options.replace_prefix:
                replace_prefix = [
                    item.split(",")
                    for item in self.options.replace_prefix
                ]
                self.logger.info(
                    f'replace_prefix for state dict: {replace_prefix}')
            else:
                replace_prefix = []

            model.load(
                self.options.load,
                omit_keys=self.options.omit_keys,
                replace_prefix=replace_prefix,
                check_loaded_options=self.options.check_loaded_options)

            self.logger.info(
                f'Finished loading model from {self.options.load}')

        if self.options.onload:
            for func in self.options.onload:
                try:
                    getattr(model, func)()
                    self.logger.info('Called function {func!s} for model')
                except BaseException:
                    self.logger.info('Calling function {func!s} failed!')
                    raise
        if self.options.use_fp16:
            old_step = model.step
            model = FP16Model(self.option_map_for_model, params, model)
            model.step = old_step
        if torch.cuda.is_available() and \
           self.options.gpu is not None and \
           self.options.gpu >= 0:
            model.cuda(self.options.gpu)

        return model

    def _on_get_args(self, *args, **kwargs):
        warnings.warn(
            ('_on_get_args is deprecated, get rid of this as soon as old '
             'model files are no longer needed'),
            DeprecationWarning)


_load_env_logger = logging.stderr_color_mt('rlpytorch.model_loader.load_env')


def load_env(
        envs,
        num_models=None,
        overrides=None,
        additional_to_load=None):
    """Load envs.

    Envs will be specified as environment variables. Specifically, the
    environment variables ``game``, ``model_file`` and ``model`` are
    required.

    ``additional_to_load`` is a dict with the following format:

        {'variable_name': (option_spec, callable)}

    For each element in ``additional_to_load``, ``load_env`` will parse
    the ``option_spec``, pass the resulting option map to ``callable``,
    and store the result of ``callable`` in the return value
    (under the key ``name``).

    Returns:
        env: dict of
            ``game`` : game module
            ``method``: Learning method used
            ``model_loaders``: loaders for model
    """
    logger = _load_env_logger
    logger.info('Loading env')

    game_loader_class = load_module(envs["game"]).Loader
    model_file = load_module(envs["model_file"])
    # TODO This is not good, need to fix.
    if len(model_file.Models[envs["model"]]) == 2:
        model_class, method_class = model_file.Models[envs["model"]]
        sampler_class = Sampler
    else:
        model_class, method_class, sampler_class = \
            model_file.Models[envs["model"]]

    overrides = dict(overrides) if overrides else {}
    overrides.update(getattr(model_file, "Overrides", {}))

    option_spec = PyOptionSpec()
    option_spec.merge(PyOptionSpec.fromClasses((
        logging.GlobalLoggingConfigurator,
        game_loader_class,
        method_class,
        sampler_class,
        ModelInterface,
    )))
    if num_models is None:
        option_spec.merge(ModelLoader.get_option_spec(model_class))
    else:
        for i in range(num_models):
            option_spec.merge(
                ModelLoader.get_option_spec(model_class, model_idx=i))
    if additional_to_load:
        for additional_option_spec, _ in additional_to_load.values():
            option_spec.merge(additional_option_spec)

    option_map = option_spec.parse(overrides=overrides)

    global_logger_configurator = logging.GlobalLoggingConfigurator(option_map)
    global_logger_configurator.configure()

    pretty_option_str = pprint.pformat(option_map.getOptionDict(), width=50)
    logger.info(f'Parsed options: {pretty_option_str}')

    game = game_loader_class(option_map)
    method = method_class(option_map)
    sampler = sampler_class(option_map)
    mi = ModelInterface(option_map)

    # You might want multiple models loaded.
    if num_models is None:
        model_loaders = [ModelLoader(option_map, model_class)]
    else:
        model_loaders = [ModelLoader(option_map, model_class, model_idx=i)
                         for i in range(num_models)]

    env = dict(
        game=game,
        method=method,
        sampler=sampler,
        model_loaders=model_loaders,
        mi=mi,
    )
    if additional_to_load:
        for name, (_, option_map_callable) in additional_to_load.items():
            env[name] = option_map_callable(option_map)

    logger.info('Finished loading env')

    return env
