![LUX Logo](src/qt/res/images/lux_logo_horizontal.png)

EMPOWERING PEOPLE

Luxcore is GNU AGPLv3 licensed.

[![Build Status](https://travis-ci.org/LUX-Core/lux.svg?branch=master)](https://travis-ci.org/LUX-Core/lux) [![GitHub version](https://badge.fury.io/gh/LUX-Core%2Flux.png)](https://badge.fury.io/gh/LUX-Core%2Flux.png) [![HitCount](http://hits.dwyl.io/LUX-Core/lux.svg)](http://hits.dwyl.io/LUX-Core/lux)
<a href="https://discord.gg/ndUg9va"><img src="https://discordapp.com/api/guilds/364500397999652866/embed.png" alt="Discord server" /></a> <a href="https://twitter.com/intent/follow?screen_name=LUX_COIN"><img src="https://img.shields.io/twitter/follow/LUX_COIN.svg?style=social&logo=twitter" alt="follow on Twitter"></a>
                                                                                                                                                     
[![Build history](https://buildstats.info/travisci/chart/LUX-Core/lux?branch=master)](https://travis-ci.org/LUX-Core/lux?branch=master)

[Website](https://luxcore.io) — [Luxgate](https://luxcore.io/luxgate/) - [PoS Web Wallet](https://www.luxcore.io/poswebwallet/) — [Block Explorer](https://explorer.luxcore.io/) — [Blog](https://reddit.com/r/LUXCoin) — [Forum](https://bitcointalk.org/index.php?topic=2254046.0) — [Telegram](https://t.me/LUXcoinOfficialChat) — [Twitter](https://twitter.com/LUX_Coin)

Features
=============

* Hybrid PoW/PoS
* Hybrid masternode
* Smart contract
* New RX2 algorithm
* Luxgate


Developed by Luxcore, LUX (LUX Coin) is a hybrid cryptocurrency utilizing both proof-of-stake and proof-of-work algorithms to enhance blockchain security and decentralization. Luxcore's hybrid consensus model employs masternodes to provide specialized functions and further secure the chain while providing additional rewards to coin holders. The LUX blockchain enables developers to utilize smart contracts and decentralized applications.

Luxcore continues to develop products for the LUX blockchain including Luxgate (a trustless, peer to peer decentralized exchange) and Luxedge (a decentralized software development platform and repository).

LUX was created in 2017 as a fork of the Bitcoin codebase, and aims to introduce new innovations and services to the broader crypto community with cross-chain and decentralized solutions.


## Coin Specifications

| Specification | Value |
|:-----------|:-----------|
| Total Blocks | `6,000,000` |
| Block Size | `4MB` |
| Block Time | `60s` |
| PoW Block Time | `120s`   |
| PoW Reward | `5 (POW) + 2 (MN) + 1 (DEV FEE)` |
| PoS Reward | `1 LUX` |
| Stake Time | `36 hours` | 
| Masternode Requirement | `16,120 LUX` |
| Masternode Reward | `25% PoW/PoS` |
| Port | `26969` |
| RPC Port | `9888` |
| Masternode Port | `26969` |
| Lux legacy address start with | `L` |
| p2sh-segwit address start with | `S` |
| Bech32 address start with | `bc` |

* NOTE: "getrawchangeaddress p2sh-segwit" to get p2sh-segwit address 

Instructions
-----------
* [Win-Lux-qt](https://docs.luxcore.io/docs/user-guide-windows)

* [Mac-Lux-qt](https://docs.luxcore.io/docs/user-guide-mac)

* [Lux Masternode](https://docs.luxcore.io/docs/how-to-setup-a-masternode)


Build Lux wallet
----------

### Building for 32-bit Windows

The next three steps are an example of how to acquire the source and build in an appropriate way.
        
Acquire the source and install dependencies.

    git clone https://github.com/LUX-Core/lux
    sudo chmod -R a+rw lux
    cd lux/depends
    ./install-dependencies.sh
    
Set the default mingw-w32 g++ compiler option to auto (option 0) by default.

    sudo update-alternatives --config i686-w64-mingw32-g++
    
Build in the usual way.

    ./build-wins.sh
    
### Building for 64-bit Windows   

The next three steps are an example of how to acquire the source and build in an appropriate way.
        
Acquire the source and install dependencies.

    git clone https://github.com/LUX-Core/lux
    sudo chmod -R a+rw lux
    cd lux/depends
    ./install-dependencies.sh
    
Set the default mingw-w64 g++ compiler option to posix (option 1).

    sudo update-alternatives --config x86_64-w64-mingw32-g++
    
Build in the usual way.

    ./build-wins.sh x64

### Build on Ubuntu

Use

    sudo add-apt-repository ppa:bitcoin/bitcoin; git clone https://github.com/LUX-Core/lux; cd lux; depends/install-dependencies.sh; ./autogen.sh; ./configure --disable-tests --with-boost-libdir=/usr/local/lib; make clean; make -j$(nproc)


Add bitcoin repository for Berkeley DB 4.8

    sudo add-apt-repository ppa:bitcoin/bitcoin

Clone lux repository

    git clone https://github.com/LUX-Core/lux

Build lux 

    cd lux
    ./depends/install-dependencies.sh
    ./autogen.sh
    ./configure --disable-tests
    make -j$(nproc)

### Build on OSX

The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

#### Preparation

Install the OS X command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

If you're running macOS Mojave 10.14/Xcode 10.0 or later, and want to use the depends system, you'll also need to use the following script to install the macOS system headers into /usr/include.

    open /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg

Then install [Homebrew](https://brew.sh)

    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

#### Dependencies

    brew install cmake automake berkeley-db4 leveldb libtool boost@1.64 --c++11 --without-single --without-static miniupnpc openssl pkg-config protobuf qt5 libevent imagemagick --with-librsvg

Link boost 1.64

    brew link boost@1.64 --force

#### Build Luxcore

Clone the Lux source code and cd into lux

        git clone https://github.com/LUX-Core/lux
        cd lux
        ./building/mac/requirements.sh
        ./building/mac/build.sh

Setup and Build: Arch Linux
-----------------------------------
This example lists the steps necessary to setup and build a command line only, non-wallet distribution of the latest changes on Arch Linux:

    pacman -S git base-devel boost libevent python
    git clone https://github.com/LUX-Core/lux
    cd lux/
    ./autogen.sh
    ./configure --without-miniupnpc --disable-tests
    make -j$(nproc)

Note:
Enabling wallet support requires either compiling against a Berkeley DB newer than 4.8 (package `db`) using `--with-incompatible-bdb`,
or building and depending on a local version of Berkeley DB 4.8. The readily available Arch Linux packages are currently built using
`--with-incompatible-bdb` according to the
As mentioned above, when maintaining portability of the wallet between the standard Bitcoin Core distributions and independently built
node software is desired, Berkeley DB 4.8 must be used.


ARM Cross-compilation
-------------------
These steps can be performed on, for example, an Ubuntu VM. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install the build requirements mentioned above.
Then, install the toolchain and curl:

    sudo apt-get install g++-arm-linux-gnueabihf curl

To build executables for ARM:

    cd depends
    make HOST=arm-linux-gnueabihf NO_QT=1
    cd ..
    ./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc++
    make -j$(nproc)

For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.

Building on FreeBSD
--------------------

Clang is installed by default as `cc` compiler, this makes it easier to get
started than on [OpenBSD](build-openbsd.md). Installing dependencies:

    pkg install autoconf automake libtool pkgconf
    pkg install boost-libs openssl libevent
    pkg install gmake

You need to use GNU make (`gmake`) instead of `make`.
(`libressl` instead of `openssl` will also work)

For the wallet (optional):

    ./contrib/install_db4.sh `pwd`
    setenv BDB_PREFIX $PWD/db4

Then build using:

    ./autogen.sh
    ./configure BDB_CFLAGS="-I${BDB_PREFIX}/include" BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx"
    gmake

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/LUX-Core/lux/tags) are created
regularly to indicate new official, stable release versions of Lux.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).


Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

### Issue

 We try to prompt our users for the basic information We always need for new issues.
 Please fill out the issue template below and submit it to our discord channel
  <a href="https://discord.gg/ndUg9va"><img src="https://discordapp.com/api/guilds/364500397999652866/embed.png" alt="Discord server" /></a>
 
 [ISSUE_TEMPLATE](doc/template/ISSUE_TEMPLATE_example.md)

## License
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2FLUX-Core%2Flux.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2FLUX-Core%2Flux?ref=badge_large)
