#!/bin/bash
set -e
set -x
cd $(dirname $0)/../

cmake -B build -DBUILD_STATIC:BOOL=TRUE -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build
tar -cpvzf ptr89.tar.gz ./ptr89
