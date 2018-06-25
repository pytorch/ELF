FROM ubuntu:18.04

# Due to nVidia not officially supporting CUDA 9.0 in Ubuntu 18.04
# CUDA 9.0 is manually installed from the Ubuntu 16.04 repositories
# If in the future Nvidia releases CUDA 9.0 on Ubuntu 18.04, you can skip this part
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates apt-transport-https gnupg2 curl && \
        curl -fsSL https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1604/x86_64/7fa2af80.pub | apt-key add - && \
        echo "deb https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1604/x86_64 /" > /etc/apt/sources.list.d/cuda.list && \
        echo "deb https://developer.download.nvidia.com/compute/machine-learning/repos/ubuntu1604/x86_64 /" > /etc/apt/sources.list.d/nvidia-ml.list

ENV CUDA_VERSION 9.0.176
ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility
ENV NVIDIA_REQUIRE_CUDA "cuda>=9.0"

ENV NCCL_VERSION 2.2.12
ENV CUDNN_VERSION 7.1.4.18
ENV CUDA_PKG_VERSION 9-0=$CUDA_VERSION-1

ENV ELF_FOLDER /go-elf
ENV MINICONDA_INSTALL_SCRIPT_NAME Miniconda3.sh

# nvidia-docker 1.0
LABEL com.nvidia.volumes.needed="nvidia_driver"
LABEL com.nvidia.cuda.version="${CUDA_VERSION}"
LABEL com.nvidia.cudnn.version="${CUDNN_VERSION}"

RUN apt-get update && apt-get install -y --no-install-recommends \
        cuda-cudart-$CUDA_PKG_VERSION && \
        ln -s cuda-9.0 /usr/local/cuda


RUN echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf && \
echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf

ENV PATH /usr/local/nvidia/bin:/usr/local/cuda/bin:${PATH}
ENV LD_LIBRARY_PATH /usr/local/nvidia/lib:/usr/local/nvidia/lib64

# install CUDA libs
RUN apt-get update && apt-get install -y --no-install-recommends \
        cuda-libraries-$CUDA_PKG_VERSION \
        cuda-cublas-9-0=9.0.176.3-1 \
        libnccl2=$NCCL_VERSION-1+cuda9.0

RUN apt-get update && apt-get install -y --no-install-recommends \
        libcudnn7=$CUDNN_VERSION-1+cuda9.0 && \
        rm -rf /var/lib/apt/lists/*

# CUDA Has been fully installed, now install the dependencies for ELF

RUN mkdir -p ${ELF_FOLDER}

RUN apt update -y && apt install -y cmake git libboost-all-dev libzmq3-dev

ADD https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh ${ELF_FOLDER}/${MINICONDA_INSTALL_SCRIPT_NAME}
RUN chmod +x ${ELF_FOLDER}/${MINICONDA_INSTALL_SCRIPT_NAME}
RUN ${ELF_FOLDER}/${MINICONDA_INSTALL_SCRIPT_NAME} -b
ENV PATH="${PATH}:/root/miniconda3/bin"
RUN conda install -c pytorch pytorch-nightly cuda90 numpy zeromq pyzmq

RUN mkdir -p ${ELF_FOLDER}/ELF
ADD . ${ELF_FOLDER}/ELF
WORKDIR ${ELF_FOLDER}/ELF

RUN git submodule sync && git submodule update --init --recursive 

# Use the Conda environment to compile ELF
RUN bash -c "source activate base && make -j4"

# Install the pretrained model
ADD https://github.com/pytorch/ELF/releases/download/pretrained-go-19x19-v0/pretrained-go-19x19-v0.bin ${ELF_FOLDER}/ELF

# Set up the interactive environment
CMD bash
