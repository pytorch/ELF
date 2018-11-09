egrep --text -i won *dgacheck*log* | sed 's/black.*black/0/g' | sed 's/white.*white/0/g' | sed 's/black.*white/1/g' | sed 's/white.*black/1/g' | sed 's/won//g' | sort | uniq -c
