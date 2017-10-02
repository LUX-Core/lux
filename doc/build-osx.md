# Mac OS X Bhcoind Build Instructions

Copyright (c) 2009-2012 Bitcoin Developers
Distributed under the MIT/X11 software license, see the accompanying file
license.txt or http://www.opensource.org/licenses/mit-license.php.  This
product includes software developed by the OpenSSL Project for use in the
OpenSSL Toolkit (http://www.openssl.org/).  This product includes cryptographic
software written by Eric Young (eay@cryptsoft.com) and UPnP software written by
Thomas Bernard.

Bhcoin authors:

- Laszlo Hanyecz <solar@heliacal.net>
- Douglas Huff <dhuff@jrbobdobbs.org>

## Notes

* See readme-qt.rst for instructions on building Bhcoin QT, the
graphical user interface.

* Tested on 10.5 and 10.6 intel.  PPC is not supported because it's big-endian.

* All of the commands should be executed in Terminal.app.. it's in
/Applications/Utilities

## Prerequisites

You need to install XCode with all the options checked so that the compiler and
everything is available in /usr not just /Developer. 
You can get the current version from http://developer.apple.com

## Installing depencies with MacPorts

1.  Download and install MacPorts from http://www.macports.org/. For 10.7 Lion: edit /opt/local/etc/macports/macports.conf and uncomment "build_arch i386".
2.  Install dependencies:

		sudo port install boost db48 openssl miniupnpc

Optionally install qrencode (and set USE_QRCODE=1):
sudo port install qrencode

## Installing dependencies with Homebrew

Homebrew is an alternative to MacPorts. If you're using MacPorts, skip this section.

1. Download and install Homebrew: https://brew.sh/
2. Install dependencies:

		brew install leveldb berkeley-db4 boost miniupnpc openssl

## Building Bhcoin

    git clone https://github.com/bhcoind/source bhcoin
    cd bhcoin/src
    make -f makefile.osx

## Useful commands

    ./bhcoind --help  # for a list of command-line options.
    ./bhcoind -daemon # to start the bhcoin daemon.
    ./bhcoind help    # When the daemon is running, to get a list of RPC commands
