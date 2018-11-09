#!/bin/bash


ROOTDIR=`git rev-parse --show-toplevel`
for u in `find ${ROOTDIR} \( -name '*.txt' -o -name '*.sh' -o -name '*.py' -o -name '*.cc' -o -name '*.h' \) -not -path "*codingenv*" -not -path "*third_party*" -exec grep -iH "$1" {} \; | sed 's/:.*//g' | sort | uniq`
do
   vim $u
   sleep 1
done
#find ~/newtasks -name "third_party" -prune \( -name '*.txt' -o -name '*.sh' -o -name '*.py' -o -name '*.cc' -o -name '*.h' \) -exec grep -iH "$1" {} \;
