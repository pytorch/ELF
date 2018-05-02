#!/usr/bin/env bash

# cd to parent of script's directory (i.e. project root)
cd "${0%/*}"/..

if [ -z "${DIFF_CMD}" ]; then
    if diff --color /dev/null /dev/null > /dev/null 2>&1; then
        DIFF_CMD='diff --color -U5 '
    elif colordiff /dev/null /dev/null > /dev/null 2>&1; then
        DIFF_CMD='colordiff -U5 '
    else
        DIFF_CMD='diff -U5 '
    fi
fi

CLANG_FORMAT_CMD=${CLANG_FORMAT_CMD:=clang-format}
OUTPUT=${OUTPUT:=/dev/null}
FILES=$( \
    find . -type f \( -name '*.h' -o -name '*.cc' \) \
    -not -path './.git/*' \
    -not -path './build/*' \
    -not -path './experimental/*' \
    -not -path './third_party/*' \
)

MUST_FORMAT=0
for f in ${FILES};
do
    if ! test -e ${f}; then
        continue
    fi
    diff -q ${f} <(${CLANG_FORMAT_CMD} -style=file ${f}) > ${OUTPUT}
    if test $? -ne 0;
    then
        echo '=========== C++ style error (fix with clang-format) ============='
        echo ${f} is not properly formatted. Diff of correction:
        echo '-----------------------------------------------------------------'
        MUST_FORMAT=1
        ${CLANG_FORMAT_CMD} -style=file ${f} > /tmp/$(basename $f)
        ${DIFF_CMD} $f /tmp/$(basename $f)
        echo
        echo
        echo
    fi
done

exit ${MUST_FORMAT}
