#!/bin/bash

touch ./scripts/elfgames/tasks/myserver/latest0 ./scripts/elfgames/tasks/myserver/toto.bin
rm ./scripts/elfgames/tasks/myserver/latest0
rm ./scripts/elfgames/tasks/myserver/*.bin
#module load anaconda3
set -e -x
ROOTDIR=`git rev-parse --show-toplevel`
cd $ROOTDIR
source scripts/devmode_set_pythonpath.sh
PYTHONPATH=$PYTHONPATH:$ROOTDIR/elf/:$ROOTDIR/:$ROOTDIR/build/:$ROOTDIR/src_cpp/:$ROOTDIR/build/elfgames/tasks:$ROOTDIR/build/elfgames/tasks/:$ROOTDIR:$ROOTDIR/elfgames/tasks/
#source activate /private/home/ssengupta/.conda/envs/go10
#source activate otgo12
#git submodule sync && git submodule update --init --recursive

#make -j 2>&1 | grep -i20 error | head -n 37

cd scripts/elfgames/tasks
touch log.log
rm log.log
./start_server_mini.sh
sleep 5
#${ROOTDIR}/viewserverlog.sh
cat ../../../log.log
