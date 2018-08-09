#!/usr/bin/env bash

export ELF_DEVELOPMENT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )"/.. && pwd )"
export PYTHONPATH="${ELF_DEVELOPMENT_ROOT}"/src_py/:"${ELF_DEVELOPMENT_ROOT}"/build/elf/:"${ELF_DEVELOPMENT_ROOT}"/build/elfgames/go/:"${ELF_DEVELOPMENT_ROOT}"/build/elfgames/tutorial/:"${ELF_DEVELOPMENT_ROOT}"/build/elfgames/tutorial_distri/:${PYTHONPATH}
