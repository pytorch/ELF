#!/bin/bash

ROOTDIR=`git rev-parse --show-toplevel`
cd $ROOTDIR/src_cpp/elfgames/tasks
find . \( -iname '*.cc' -o -iname '*.h' -o -iname '*.py' \) -exec  echo -n {} \; -exec diff {} `echo {} | sed 's/^/..\/go\//g'` \; 2>&1 | grep -iv codingenv



