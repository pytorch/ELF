#!/bin/bash

module load anaconda3
ROOTDIR=`git rev-parse --show-toplevel`
cd $ROOTDIR
source scripts/devmode_set_pythonpath.sh
#/private/home/oteytaud/newgo/src_py/:/private/home/oteytaud/newgo/build/elf/:/private/home/oteytaud/newgo/build/elfgames/go/:/private/home/oteytaud/newgo/src_py/:/private/home/oteytaud/newgo/build/elf/:/private/home/oteytaud/newgo/build/elfgames/go/:
PYTHONPATH=$PYTHONPATH:$ROOTDIR/elf/:$ROOTDIR/:$ROOTDIR/build/:$ROOTDIR/src_cpp/:$ROOTDIR/build/elfgames/go:$ROOTDIR/build/elfgames/go/:$ROOTDIR:$ROOTDIR/elfgames/go/
source activate /private/home/ssengupta/.conda/envs/go10
#git submodule sync && git submodule update --init --recursive

pid=`ps -ef | grep -i selfplay.py | grep batchsize | cut -c 9-16`
if [ -z "$pid" ]
then
   echo no such process
else
   echo process $pid
   kill -15 $pid
fi
make -j 2>&1 | grep -i20 error | head -n 37

cd scripts/elfgames/go
#rm log.log
./start_client.sh
