#!/bin/bash
set -e
set -x
cd $(dirname $0)/../

[[ -d ./build-win ]] || mkdir build-win

cmake -B build-win -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32.cmake -DBUILD_STATIC:BOOL=TRUE
cmake --build build-win -- -j$(nproc)

ls -lah build-win/ptr89.exe
