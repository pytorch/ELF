ROOTDIR=`git rev-parse --show-toplevel`
find $ROOTDIR \( -name '*.txt' -o -name '*.sh' -o -name '*.py' -o -name '*.cc' -o -name '*.h' \) -not -path "*codingenv*" -not -path "*third_party*" -exec grep -iH "$1" {} \;
#find ~/newtasks -name "third_party" -prune \( -name '*.txt' -o -name '*.sh' -o -name '*.py' -o -name '*.cc' -o -name '*.h' \) -exec grep -iH "$1" {} \;
