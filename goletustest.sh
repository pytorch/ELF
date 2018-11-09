#!/bin/bash

module load anaconda3
set -e -x
ROOTDIR=`git rev-parse --show-toplevel`
cd $ROOTDIR
source scripts/devmode_set_pythonpath.sh
PYTHONPATH=$PYTHONPATH:$ROOTDIR/elf/:$ROOTDIR/:$ROOTDIR/build/:$ROOTDIR/src_cpp/:$ROOTDIR/build/elfgames/go:$ROOTDIR/build/elfgames/go/:$ROOTDIR:$ROOTDIR/elfgames/go/
source activate /private/home/ssengupta/.conda/envs/go10
#git submodule sync && git submodule update --init --recursive

pid=`ps -ef | grep -i train.py | grep batchsize | grep num_games | grep selfplay_async | cut -c 9-16`
if [ -z "$pid" ]
then
   echo no such process
else
   echo process $pid
   kill -15 $pid
fi
make -j 2>&1 | grep -i20 error | head -n 37

cd scripts/elfgames/go
touch log.log
rm log.log
./start_server.sh
sleep 5
#${ROOTDIR}/viewserverlog.sh
cat log.log
