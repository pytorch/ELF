#!/bin/bash




ROOTDIR=`git rev-parse --show-toplevel`
for i in `find $ROOTDIR \( -name '*.txt' -o -name '*.sh' -o -name '*.py' -o -name '*.cc' -o -name '*.h' \) -not -path "*codingenv*" -not -path "*third_party*" -exec grep -iH "$1" {} \; | sed 's/:.*//g'`
do
echo $i
vim $i
sleep 1
done
