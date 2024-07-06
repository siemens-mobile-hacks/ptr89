#!/bin/bash
set -e
set -x
cd $(dirname $0)/../

[[ -d ./build-win ]] || mkdir build-win

cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32.cmake
make -j$(nproc)

ls -lah ptr89.exe
