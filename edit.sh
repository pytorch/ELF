#!/bin/bash
ROOTDIR=`git rev-parse --show-toplevel`

# Find a file with name $1 and edit it (first pass: only with tasks in the path)
for i in `find ${ROOTDIR}/ -iname $1 | grep "\/task"`
do
echo $i
vim $i
done
# Find a file with name $1 and edit it (second pass)
for i in `find ${ROOTDIR}/ -iname $1 | grep -v "\/task"`
do
echo $i
vim $i
done
#find ~/newtasks -iname $1 
