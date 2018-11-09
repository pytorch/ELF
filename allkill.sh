#!/bin/bash

# Killing server.
pid=`ps -ef | grep -i train.py | grep batchsize | grep num_games | grep selfplay_async | cut -c 9-16`
if [ -z "$pid" ]
then
   echo no such process
else
   echo process $pid
   kill -15 $pid
fi
# Killing checkers..
pid=`ps -ef | grep -i check_mini | cut -c 9-16`
if [ -z "$pid" ]
then
   echo no such process
else
   echo process $pid
   kill -15 $pid
fi

# Killing clients.
pid=`ps -ef | grep -i selfplay.py | grep batchsize | cut -c 9-16`
if [ -z "$pid" ]
then
   echo no such process
else
   echo process $pid
   kill -15 $pid
fi

