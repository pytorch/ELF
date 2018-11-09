#!/bin/bash

for i in cc h
do
   for f in `find . -iname "*.$i" | grep -v third_par`
   do
       g=`echo $f | sed 's/^\./\/private\/home\/oteytaud\/newtasks/g'`
       numdifs=`diff $f $g | wc -l`
       if [ "$numdifs" -ne "0" ];
       then
           echo $f: $numdifs
       fi
   done
done
