#!/bin/bash

date

#################################################################
# Update Ubuntu and install prerequisites for running LUX   #
#################################################################
sudo add-apt-repository ppa:bitcoin/bitcoin
sudo apt-get update

#################################################################
# Lux QT wallet are using QT5. So, QT4 must be removed
#################################################################
sudo apt purge qt4-qmake libqt4-dev -y

#################################################################
# Remove incompatible libdb
#################################################################
sudo apt purge libdb++-dev libdb-dev -y

#################################################################
# Remove unused package
#################################################################
sudo apt autoremove -y

#################################################################
# Build LUX from source                                     #
#################################################################
NPROC=$(nproc)
echo "nproc: $NPROC"

set -e

#################################################################
# Install all necessary packages for building LUX         #
#################################################################
sudo apt-get install -y libdb4.8-dev libdb4.8++-dev libzmq3-dev libminiupnpc-dev libcrypto++-dev libboost-all-dev build-essential libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev ufw git software-properties-common libtool autotools-dev autoconf pkg-config libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev automake g++-mingw-w64-x86-64 libevent-dev bsdmainutils curl git g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64

