#!/bin/bash


module unload gcc/5.2.0
module load cuda/9.0
module load cudnn/v7.0-cuda.9.0
# module load NCCL/2.1.2-cuda.9.0
module load NCCL/2.2.12-1-cuda.9.0
module load anaconda3/5.0.1
module load zeromq/4.2.1/gcc.4.8.4
module load gcc/7.1.0
#source activate otgo12
source activate /private/home/ssengupta/.conda/envs/go10
export NCCL_MAX_NRINGS=1
export NCCL_DEBUG=INFO

scripts/devmode_set_pythonpath.sh
#module load anaconda3
#module load gcc/7.1.0
#module load cuda/9.0
#module load cudnn/v7.0-cuda.9.0
#source activate otgo12
#conda install -y numpy zeromq pyzmq --user
#conda install -y -c pytorch pytorch cuda90 --user


#module load anaconda3
#module load cuda/9.0
#module load cudnn/v7.0-cuda.9.0
#source activate otgo12
#conda install numpy zeromq pyzmq
#conda install -c pytorch pytorch cuda90

./allkill.sh

set -e -x
#make -j

# Launch the server.
./letustest.sh  
#sleep 1

# Below, once per client: this launches something for checking how much we win against the baseline.
#./letustest_check.sh &

#sleep 1
./letustest_client.sh &

#sleep 90 

#./allkill.sh
wait

