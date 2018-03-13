#!/bin/sh
set -x -e

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DEVMJIT_INCLUDE_EXAMPLES=On -DEVMJIT_TESTS=On ..
cmake --build .
cmake --build . --target evmjit-standalone
cmake --build . --target example-capi
