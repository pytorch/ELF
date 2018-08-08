.. footer::

    Copyright |copy| 2018-present, Facebook, Inc. |---|
    all rights reserved.

.. |copy| unicode:: 0xA9
.. |---| unicode:: U+02014

===
ELF
===

ELF is an Extensive, Lightweight, and Flexible platform for game research. We have used it to build our Go playing bot, ELF OpenGo, which achieved a 14-0 record versus four global top-30 players in April 2018. The final score is 20-0 (each professional Go players play 5 games).

We have released our v0 models `here`__.

__ https://github.com/pytorch/ELF/releases

This version is a work-in-progress successor to the `original ELF platform`__.

__ https://github.com/facebookresearch/ELF

**DISCLAIMER**: this code is early research code. What this means is:

- It may not work reliably (or at all) on your system.
- The code quality and documentation are quite lacking, and much of the code might still feel "in-progress".
- There are quite a few hacks made specifically for our systems and infrastructure.

Although we intend to release significant improvements over the next month, we're a small team so your patience is greatly appreciated.

|build|

.. |build| image:: https://circleci.com/gh/pytorch/ELF.png?style=shield

License
=======

ELF is released under the BSD-style licence found in the ``LICENSE`` file.

Citing ELF
==========

If you use ELF in your research, please consider citing the original NIPS paper as follows::

    @inproceedings{tian2017elf,
      author = {Yuandong Tian and Qucheng Gong and Wenling Shang and Yuxin Wu and C. Lawrence Zitnick},
      title = {ELF: An extensive, lightweight and flexible research platform for real-time strategy games},
      booktitle = {Advances in Neural Information Processing Systems},
      pages = {2656--2666},
      year = {2017}
    }

If you use ELF OpenGo or OpenGo-like functionality, please consider citing the library as follows::

    @misc{ELFOpenGo2018,
      author = {Yuandong Tian and {Jerry Ma*} and {Qucheng Gong*} and Shubho Sengupta and Zhuoyuan Chen and C. Lawrence Zitnick},
      title = {ELF OpenGo},
      year = {2018},
      journal = {GitHub repository},
      howpublished = {\url{https://github.com/pytorch/ELF}}
    }

\* Qucheng Gong and Jerry Ma equally contributed as second authors.

\*\* We also thank Yuxin Wu for his help on this project.

Dependencies
============

We run ELF using:

- Ubuntu **18.04**
- Python **3.6**
- GCC **7.3**
- CUDA **9.0**
- CUDNN **7.0**
- NCCL **2.1.2**

At the moment, this is the only supported environment. Other environments may also work, but we unfortunately do not have the manpower to investigate compatibility issues.

Here are the dependency installation commands for Ubuntu 18.04 and conda::

    sudo apt-get install cmake g++ gcc libboost-all-dev libzmq3-dev
    conda install numpy zeromq pyzmq

    # From the project root
    git submodule sync && git submodule update --init --recursive

You also need to install PyTorch 0.4.1 or later::

    conda install -c pytorch pytorch cuda90

A Dockerfile has been provided if you wish to build ELF using Docker.

Building
========

``cd`` to the project root and run ``make`` to build.

Testing
=======

After building, ``cd`` to the project root and run ``make test`` to test.

Using ELF
=========

Currently, ELF must be run straight from source. You'll need to run ``source scripts/devmode_set_pythonpath.sh`` to augment ``$PYTHONPATH`` appropriately.

Training a Go bot
-----------------

To train a model, please follow these steps:

1) Build ELF and run ``source scripts/devmode_set_pythonpath.sh`` as described above.

2) Change directory to ``scripts/elfgames/go/``

3) Edit ``server_addrs.py`` to specify the server's IP address. This is the machine that will train the neural network.

4) Create the directory where the server will write the model directory. This defaults to ``myserver``

5) Run ``start_server.sh`` to start the server. We have tested this on a machine with 8 GPUs.

6) Run ``start_client.sh`` to start the clients. The clients should be able to read the model written by the server, so the clients and the server need to mount the same directory via NFS. We have tested this on 2000 clients, each running exclusively on one GPU.

Running a Go bot
----------------

Here is a basic set of commands to run and play the bot via the GTP protocol:

1) Build ELF and run ``source scripts/devmode_set_pythonpath.sh`` as described above.

2) Train a model, or grab a pretrained model from the repository's Github "Releases" tab.

3) Change directory to ``scripts/elfgames/go/``

4) Run ``./gtp.sh path/to/modelfile.bin --verbose --gpu 0 --num_block 20 --dim 224 --mcts_puct 1.50 --batchsize 16 --mcts_rollout_per_batch 16 --mcts_threads 2 --mcts_rollout_per_thread 8192 --resign_thres 0.05 --mcts_virtual_loss 1``

We've found that the above settings work well for playing the bot. You may change ``mcts_rollout_per_thread`` to tune the thinking time per move.

After the environment is set up and the model is loaded, you can start to type gtp commands to get the response from the engine.

Analysis mode
-------------

Here is the command to analyze an existing sgf file:

1) Build ELF and run ``source scripts/devmode_set_pythonpath.sh`` as described above.

2) Train a model, or grab a pretrained model from the repository's Github "Releases" tab.

3) Change directory to ``scripts/elfgames/go/``

4) Run ``./analysis.sh /path/to/model --preload_sgf /path/to/sgf --preload_sgf_move_to [move_number] --dump_record_prefix [tree] --verbose --gpu 0 --mcts_puct 1.50 --batchsize 16 --mcts_rollout_per_batch 16 --mcts_threads 2 --mcts_rollout_per_thread 8192 --resign_thres 0.0 --mcts_virtual_loss 1 --num_games 1``

The settings for rollouts are similar as above. The process should run automatically after loading the environment, models and previous moves. You should see the move suggested by the AI after each move, along with its value and prior. This process will also generate a lot of tree files, prefixed with ``tree`` (you can change it with ``--dump_record_prefix`` option above.) The tree files will contain the full search at each move along with its prior and value. To abort the process simply kill it as the current implementation will run it to the end of the game. 
