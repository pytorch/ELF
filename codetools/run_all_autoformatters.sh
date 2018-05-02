#!/usr/bin/env bash

# cd to parent of script's directory (i.e. project root)
cd "${0%/*}"/..

./codetools/autoformat_cpp_style.sh
./codetools/autoformat_py_style.sh
