#!/bin/sh
set -e

SRC_DIR=$(dirname $(pwd))

CONTAINER=$(docker run -d -v $SRC_DIR:/src:ro chfast/cpp-ethereum-dev tail -f /dev/null)

docker exec $CONTAINER sh -c 'mkdir build && cd build && cmake /src -DLLVM_DIR=/usr/lib/llvm-3.9/lib/cmake/llvm'
docker exec $CONTAINER cmake --build /build

docker kill $CONTAINER
