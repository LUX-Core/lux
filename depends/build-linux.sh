#!/bin/sh
#-###############################################-#
# C++ Cross-Compiler - The Luxcore Developer-2018 #
#-###############################################-#

# Set platform variables
PLATFORM="arm-linux-gnueabihf"
if [ "$1" = "x32" ]; then
    PLATFORM="aarch64-linux-gnu"
fi
# Install development tools if needed
if [ ! -f $CC ]; then
   sudo apt-get install software-properties-common && sudo add-apt-repository ppa:bitcoin/bitcoin && sudo apt-get update && sudo apt-get install -y libdb4.8-dev libdb4.8++-dev libzmq3-dev libminiupnpc-dev libcrypto++-dev libboost-all-dev build-essential libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev ufw git software-properties-common libtool autotools-dev autoconf pkg-config libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev automake g++-mingw-w64-x86-64 libevent-dev
fi

# Make dependencies
make HOST=$PLATFORM

cd ..

./autogen.sh Windows $PLATFORM $INSTALL_DIR && ./configure --prefix=$PWD/depends/$PLATFORM --host=$PLATFORM --disable-tests && make clean && make -j$(nproc)

