#!/usr/bin/env bash

# cd to parent of script's directory (i.e. project root)
cd "${0%/*}"/..

./codetools/check_cpp_style.sh
./codetools/check_py_style.sh
