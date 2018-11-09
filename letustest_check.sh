#!/bin/bash

set -e -x
#touch ./scripts/elfgames/tasks/myserver/latest0 ./scripts/elfgames/tasks/myserver/toto.bin
#rm ./scripts/elfgames/tasks/myserver/latest0
#rm ./scripts/elfgames/tasks/myserver/*.bin

#module load anaconda3
ROOTDIR=`git rev-parse --show-toplevel`
cd $ROOTDIR
source scripts/devmode_set_pythonpath.sh
#/private/home/oteytaud/newtasks/src_py/:/private/home/oteytaud/newtasks/build/elf/:/private/home/oteytaud/newtasks/build/elfgames/go/:/private/home/oteytaud/newtasks/src_py/:/private/home/oteytaud/newtasks/build/elf/:/private/home/oteytaud/newtasks/build/elfgames/go/:
PYTHONPATH=$PYTHONPATH:$ROOTDIR/elf/:$ROOTDIR/:$ROOTDIR/build/:$ROOTDIR/src_cpp/:$ROOTDIR/build/elfgames/tasks:$ROOTDIR/build/elfgames/tasks/:$ROOTDIR:$ROOTDIR/elfgames/tasks/
#source activate /private/home/ssengupta/.conda/envs/go10
#source activate otgo12
#git submodule sync && git submodule update --init --recursive

#pid=`ps -ef | grep -i selfplay.py | grep batchsize | cut -c 9-16`
#if [ -z "$pid" ]
#then
#   echo no such process
#else
#   echo process $pid
#   kill -15 $pid
#fi
#make -j 2>&1 | grep -i20 error | head -n 37

pushd scripts/elfgames/tasks
#rm log.log
(stdbuf -o 0 -i 0 ./start_check_mini.sh 2>&1 | stdbuf -o 0 -i 0  tee ${ROOTDIR}/checklogs 2>&1  ) || true
popd

echo We have finished!
#./viewchecklog.sh
tail -n 50 checklogs
