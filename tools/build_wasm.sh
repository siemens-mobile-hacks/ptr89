#!/bin/bash
set -e
set -x
cd $(dirname $0)/../

[[ -d ./build-wasm ]] || mkdir build-wasm

cd build-wasm
emcmake cmake .. -DBUILD_WASM=true
make -j$(nproc)
