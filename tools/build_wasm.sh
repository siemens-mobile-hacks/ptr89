#!/bin/bash
set -e
set -x
cd "$(dirname "$0")/../"

pnpm install --frozen-lockfile
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS:BOOL=TRUE
cmake --build build-wasm -- -j"$(nproc)"
pnpm run build
ctest --test-dir build-wasm --output-on-failure
