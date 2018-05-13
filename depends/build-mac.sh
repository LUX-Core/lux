#!/bin/sh
#-###############################################-#
# C++ Cross-Compiler - The Luxcore Developer-2018 #
#-###############################################-#

# Set platform variables
export HOST="x86_64-apple-darwin11"
export OSX_SDK="10.11"

# curl OSX-SDK
mkdir -p depends/SDKs depends/sdk-sources
cd depends/SDKs
curl -sL https://github.com/phracker/MacOSX-SDKs/releases/download/MacOSX10.11.sdk/MacOSX10.11.sdk.tar.xz | tar xJ
cd ../..

# Install development tools if needed
if [ ! -f $CC ]; then
  sudo apt-get install curl librsvg2-bin libtiff-tools bsdmainutils cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools
fi

# Make dependencies
make -C depends HOST=$HOST
cd ../..
./autogen.sh && ./configure --prefix=$PWD/depends/$PLATFORM --host=$PLATFORM --disable-tests && make clean && make -j$(nproc)
