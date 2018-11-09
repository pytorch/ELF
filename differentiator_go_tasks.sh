#!/bin/bash


list=`find . -iname 'tasks' `

for d in $list
do
	files=`find $d \( -iname '*.py' -o -iname '*.h' -o -iname '*.cc' \) | grep -v test`
	for f in $files
	do
		of=`echo $f | sed 's/\/tasks/\/go/g'`
		if [ -f $of ]; then
			cp -f $f /tmp/yo
			sed -i 's/tasks/go/g' /tmp/yo
			sed -i 's/ChouFleur/Go/g' /tmp/yo
			numdiffs=`diff /tmp/yo $of | wc -l `
	       	 	if [ "$numdiffs" -ne "0" ]
			then
			        echo "$f": $numdiffs "====================================="
				diff /tmp/yo $of
	
         	      	fi	
                fi	
	done
done
