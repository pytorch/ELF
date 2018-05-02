FROM ubuntu:bionic

WORKDIR /tmp/docker_init
SHELL ["/bin/bash", "-c"]

RUN apt-get update
RUN apt-get install -y \
    clang-format \
    clang-tidy \
    cmake \
    curl \
    g++ \
    gcc \
    git \
    libboost-all-dev \
    libzmq3-dev \
;
RUN ln -sf `which clang-format-6.0` /usr/bin/clang-format
RUN curl https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh > conda_install.sh
RUN bash conda_install.sh -p /conda -b
ENV PATH=/conda/bin:${PATH}
RUN conda update -n base conda
RUN conda create -yn elf_env python=3.6
RUN source activate elf_env
RUN conda install -y \
    flake8 \
    numpy \
    pytest \
    zeromq \
;
RUN conda install -c conda-forge autopep8
RUN conda install -c pytorch pytorch torchvision cuda90

RUN rm -rf /tmp/docker_init
