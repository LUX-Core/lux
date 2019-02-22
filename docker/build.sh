#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

DOCKER_IMAGE=${DOCKER_IMAGE:-luxcore/lux}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/luxd docker/bin/
cp $BUILD_DIR/src/lux-cli docker/bin/
cp $BUILD_DIR/src/lux-tx docker/bin/
strip docker/bin/luxd
strip docker/bin/lux-cli
strip docker/bin/lux-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
