#!/bin/sh
#-###############################################-#
# C++ Cross-Compiler - The Luxcore Developer-2018 #
#-###############################################-#

# Set platform variables
PLATFORM="i686-w64-mingw32"
if [ "$1" = "x64" ]; then
    PLATFORM="x86_64-w64-mingw32"
fi

CC="/usr/bin/$PLATFORM-gcc-posix"
CXX="/usr/bin/$PLATFORM-g++-posix"
OLD_PATH=`pwd`
INSTALL_DIR="$OLD_PATH/$PLATFORM"
LIB_DIR="$INSTALL_DIR/lib"
INCLUDE_DIR="$INSTALL_DIR/include"

#sudo apt install software-properties-common
#sudo add-apt-repository "deb http://archive.ubuntu.com/ubuntu zesty universe"
#sudo apt update
#sudo apt upgrade

# Install development tools if needed
if [ ! -f $CC ]; then
   sudo apt-get install build-essential libtool autotools-dev automake cmake pkg-config bsdmainutils curl g++-mingw-w64-x86-64 mingw-w64-x86-64-dev g++-mingw-w64-i686 mingw-w64-i686-dev -y
fi

#Double check whether we have the expected toolchain
if [ ! -f $CC ]; then
    CC=""
    CXX=""
    [ -f "/usr/bin/$PLATFORM-gcc" ] && CC="/usr/bin/$PLATFORM-gcc"
    [ -f "/usr/bin/$PLATFORM-g++" ] && CXX="/usr/bin/$PLATFORM-g++"
else
    sudo update-alternatives --set $PLATFORM-gcc /usr/bin/$PLATFORM-gcc-posix
    sudo update-alternatives --set $PLATFORM-g++ /usr/bin/$PLATFORM-g++-posix
fi

if [ -z "$CC" ] || [ -z "$CXX" ]; then
    echo "No expected toolchain existing. Quit compilation process"
    exit
fi

# Make dependencies
make HOST=$PLATFORM -j$(nproc)

# Build from source code leveldb
LEVELDB_DIR="../src/leveldb"
cd $LEVELDB_DIR
CC="$CC" CXX="$CXX" TARGET_OS="NATIVE_WINDOWS" make
cd $OLD_PATH

[ -f "$LIB_DIR/libleveldb.a" ] && rm "$LIB_DIR/libleveldb.a"
[ -f "$LIB_DIR/libmemenv.a"  ] && rm "$LIB_DIR/libmemenv.a"
[ -d "$INCLUDE_DIR/leveldb"  ] && rm -r "$INCLUDE_DIR/leveldb"
[ -f "$LEVELDB_DIR/libleveldb.a"    ] && cp "$LEVELDB_DIR/libleveldb.a" "$LIB_DIR/libleveldb.a"
[ -f "$LEVELDB_DIR/libmemenv.a"     ] && cp "$LEVELDB_DIR/libmemenv.a" "$LIB_DIR/libmemenv.a"
[ -d "$LEVELDB_DIR/include/leveldb" ] && cp -r "$LEVELDB_DIR/include/leveldb" "$INCLUDE_DIR"
mkdir "$INCLUDE_DIR/leveldb/helpers"
[ -f "$LEVELDB_DIR/helpers/memenv/memenv.h" ] && cp -r "$LEVELDB_DIR/helpers/memenv/memenv.h" "$INCLUDE_DIR/leveldb/helpers/memenv.h"
cp ../src/config/implementation.hpp "$INCLUDE_DIR/boost/unordered/detail/implementation.hpp"
cp ../src/config/condition_variable.hpp "$INCLUDE_DIR/boost/thread/win32/condition_variable.hpp"

cd ..
./autogen.sh windows $PLATFORM $INSTALL_DIR
./configure --prefix=$PWD/depends/$PLATFORM --host=$PLATFORM --disable-shared --enable-reduce-exports CPPFLAGS="-DMINIUPNP_STATICLIB" CFLAGS="-std=c99" LDFLAGS="-static -static-libgcc -Wl,-Bstatic -lstdc++"

# Build the application
make -j$(nproc) #--trace

RELEASE="$OLD_PATH/Release"
mkdir -p "$RELEASE"

# Remove the symbols for release
[ -f "./src/qt/lux-qt.exe" ] && cp "./src/qt/lux-qt.exe" "$RELEASE"
[ -f "./src/luxd.exe" ] && cp "./src/luxd.exe" "$RELEASE"
[ -f "./src/lux-cli.exe" ] && cp "./src/lux-cli.exe" "$RELEASE"


cd "$RELEASE"
/usr/bin/$PLATFORM-strip *
cd $OLD_PATH

