

ROOTDIR=`git rev-parse --show-toplevel`
find $ROOTDIR -iname $1 -exec view {} \;
find $ROOTDIR -iname $1 
