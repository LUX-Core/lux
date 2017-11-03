Copyright (c) 2009-2012 Bitcoin Developers
Distributed under the MIT/X11 software license, see the accompanying
file license.txt or http://www.opensource.org/licenses/mit-license.php.
This product includes software developed by the OpenSSL Project for use in
the OpenSSL Toolkit (http://www.openssl.org/).  This product includes
cryptographic software written by Eric Young (eay@cryptsoft.com) and UPnP
software written by Thomas Bernard.


UNIX BUILD NOTES
================

Dependencies
---------------------

These dependencies are required:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 libssl      | Crypto           | Random Number Generation
 libgmp      | Secp256k1        | Secp256k1 Dependency
 libboost    | Utility          | Library for threading, data structures, etc
 libevent    | Networking       | OS independent asynchronous networking
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 libsecp256k1| Secp256k1        | Elliptic Curve Cryptography

Optional dependencies:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 miniupnpc   | UPnP Support     | Firewall-jumping support
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 protobuf    | Payments in GUI  | Data interchange format used for payment protocol (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)

For the versions used in the release, see [release-process.md](release-process.md) under *Fetch and build inputs*.

System requirements
--------------------

C++ compilers are memory-hungry. It is recommended to have at least 1 GB of
memory available when compiling Lux Core. With 512MB of memory or less
compilation will take much longer due to swap thrashing.

Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------
Build requirements:

    sudo apt-get install build-essential libtool automake autotools-dev autoconf pkg-config libssl-dev libgmp3-dev libevent-dev bsdmainutils

On at least Ubuntu 14.04+ and Debian 7+ there are generic names for the
individual boost development packages, so the following can be used to only
install necessary parts of boost:

    sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev

If that doesn't work, you can install all boost development packages with:

    sudo apt-get install libboost-all-dev

BerkeleyDB is required for the wallet. db4.8 packages are available [here](https://launchpad.net/~bitcoin/+archive/bitcoin).
You can add the repository and install using the following commands:

    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Ubuntu and Debian have their own libdb-dev and libdb++-dev packages, but these will install
BerkeleyDB 5.1 or later, which break binary wallet compatibility with the distributed executables which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass `--with-incompatible-bdb` to configure.

Optional:

    sudo apt-get install libminiupnpc-dev (see --with-miniupnpc and --enable-upnp-default)

Dependencies for the GUI: Ubuntu & Debian
-----------------------------------------

If you want to build Lux-Qt, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
If both Qt 4 and Qt 5 are installed, Qt 5 will be used.

To build with Qt 5 (recommended) you need the following:

    sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler

libqrencode (optional) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a lux-qt executable will be
built by default.

Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself:

```bash
TRANSFER_ROOT=$(pwd)

# Pick some path to install BDB to, here we create a directory within the lux directory
BDB_PREFIX="${TRANSFER_ROOT}/db4"
mkdir -p $BDB_PREFIX

# Fetch the source and verify that it is not tampered with
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
echo '12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef  db-4.8.30.NC.tar.gz' | sha256sum -c
# -> db-4.8.30.NC.tar.gz: OK
tar -xzvf db-4.8.30.NC.tar.gz

# Build the library and install to our prefix
cd db-4.8.30.NC/build_unix/
#  Note: Do a static build so that it can be embedded into the executable, instead of having to find a .so at runtime
../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX
make install

# Configure Lux Core to use our own-built instance of BDB
cd $TRANSFER_ROOT
./autogen.sh
./configure LDFLAGS="-L${BDB_PREFIX}/lib/" CPPFLAGS="-I${BDB_PREFIX}/include/" # (other args...)
```

Notes
-----
1) You only need Berkeley DB if the wallet is enabled (see the section *Disable-Wallet mode* below).

2) The release is built with GCC and then "strip luxd" to strip the debug
symbols, which reduces the executable size by about 90%.

To Build luxd
--------

With UPNP:

    cd src && \
    make -f makefile.unix && \
    strip luxd

(Recommended) Without UPNP:

    cd src && \
    make -f makefile.unix USE_UPNP= && \
    strip luxd

To Build Lux-QT
--------

With UPNP:
    qmake -qt=qt5 && \
    make \

(Recommended) Without UPNP:

    qmake -qt=qt5 USE_UPNP=- && \
    make \

Boost
-----
If you need to build Boost yourself:

	sudo su
	./bootstrap.sh
	./bjam install

Upnp
---------
If you need to build miniupnpc yourself:

    tar -xzvf miniupnpc-1.6.tar.gz
    cd miniupnpc-1.6
    make
    sudo su
    make install

Security
--------
To help make your lux installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, you can take the following measures:

* Position Independent Executable
    Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. An attacker who is able to cause execution of code at an arbitrary
    memory location is thwarted if he doesn't know where anything useful is located.
    The stack and heap are randomly located by default but this allows the code section to be
    randomly located as well.

    On an Amd64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: "relocation R_X86_64_32 against `......' can not be used when making a shared object;"

    To build with PIE, use:
    make -f makefile.unix ... -e PIE=1

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:
    scanelf -e ./lux

    The output should contain:
     TYPE
    ET_DYN

* Non-executable Stack
    If the stack is executable then trivial stack based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, lux should be built with a non-executable stack
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    scanelf -e ./lux

    the output should contain:
    STK/REL/PTL
    RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.
