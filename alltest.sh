#!/bin/bash

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

# Below, once per client.
#./letustest_check.sh &

#sleep 1
./letustest_client.sh &

wait

