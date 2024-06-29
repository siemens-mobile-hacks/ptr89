#!/bin/bash
set -e
set -x
[[ -d build ]] || mkdir build
cd build
cmake ..
make "$@"
