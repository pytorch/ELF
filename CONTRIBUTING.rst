===================
Contributing to ELF
===================

We want to make contributing to this project as easy and transparent as possible.

Dependencies
============

In addition to the project dependencies, you'll need the following if you'd like to contribute code to ELF2::

    sudo apt-get install clang-format clang-tidy
    conda install flake8 pytest
    conda install -c conda-forge autopep8

Pull Requests
=============

We actively welcome your pull requests.

1. Fork the repo and create your branch from ``master``.
2. If you've added code that should be tested, add tests.
3. If you've changed APIs, update the documentation.
4. Ensure the test suite passes.
5. Make sure your code lints (see the ``codetools/`` directory).
6. If you haven't already, complete the Contributor License Agreement ("CLA").

We have a continuous integration system that will double-check steps 4 and 5 for you.

Contributor License Agreement ("CLA")
=====================================

In order to accept your pull request, we need you to submit a CLA. You only need
to do this once to work on any of Facebook's open source projects.

Complete your CLA `here`__.

__ https://code.facebook.com/cla

Issues
======

We use GitHub issues to track public bugs. Please ensure your description is
clear and has sufficient instructions to be able to reproduce the issue.

This project is still an extremely unstable prototype so we will be limited in
the amount of support we're able to provide.

Coding Style
============

- For C++, we use a style very similar to the `HHVM style guide`__. This prescription is not formal, and our ``.clang-format`` file and existing code should eventually be a good source of truth. Due to a quirk of ``Python.h``, all ``pybind`` includes must be first in the include order.
- For Python, we use pep8.

__ https://github.com/facebook/hhvm/blob/master/hphp/doc/coding-conventions.md

We have tools to check your C++ and Python code in the ``codetools/`` directory

License
=======

By contributing to ELF, you agree that your contributions will be licensed
under the LICENSE file in the root directory of this source tree.
