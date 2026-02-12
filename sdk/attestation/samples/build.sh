#!/bin/bash
set -x

ROOT_DIR=$(cd $(dirname $0);pwd)

if [ ! -d "QCBOR" ]; then
    git clone https://github.com/laurencelundblade/QCBOR.git -b v1.2
fi

if [ ! -d "t_cose" ]; then
    git clone https://github.com/laurencelundblade/t_cose.git -b v1.1.2
fi

cd $ROOT_DIR/QCBOR
make

cd $ROOT_DIR/t_cose
cmake -S . -B build
cmake --build build

cd $ROOT_DIR
cmake -S . -B build
cmake --build build
