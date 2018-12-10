#!/bin/bash


egrep "esign.*won$" clientlogs | grep -v '\[' | sed 's/Player.*blackwon/1/g' | sed 's/Player.*whitewon/0/g' | grep "^.$" > /tmp/polygames_check

num=`cat /tmp/polygames_check | wc -l`

echo $num games

delta=$(( $num / 30 ))
for v in `seq 0 $delta $num`
do
    echo -n ${v}:
    res=`head -n $v /tmp/polygames_check | tail -n $delta | awk '{ sum += $1 } END { if (NR > 0) print 100 * sum / NR }' | sed 's/\..*//g'`
    for u in `seq 0 $res`
    do
           echo -n "."
    done
    echo $res
done
ls -ctrl ./scripts/elfgames/tasks/myserver/
