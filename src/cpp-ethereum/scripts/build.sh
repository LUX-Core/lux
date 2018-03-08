#!/usr/bin/env bash

#------------------------------------------------------------------------------
# Bash script to build cpp-cpp-ethereum within TravisCI.
#
# The documentation for cpp-cpp-ethereum is hosted at http://cpp-cpp-ethereum.org
#
# ------------------------------------------------------------------------------
# This file is part of cpp-cpp-ethereum.
#
# cpp-cpp-ethereum is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# cpp-cpp-ethereum is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with cpp-cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>
#
# (c) 2016 cpp-cpp-ethereum contributors.
#------------------------------------------------------------------------------

set -e -x

# There is an implicit assumption here that we HAVE to run from repo root

mkdir -p build
cd build
if [ $(uname -s) == "Linux" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=$1 -DTESTS=$2 -DCOVERAGE=On -DEVMJIT=On -DLLVM_DIR=/usr/lib/llvm-3.9/lib/cmake/llvm
else
    cmake .. -DCMAKE_BUILD_TYPE=$1 -DTESTS=$2 -DCOVERAGE=On
fi

make -j2
