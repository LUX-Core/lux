#!/bin/sh
#Version 0.0.2
#Info: Build lux wallet for linux
#Luxcoin Version 4.0.0 or above
#Tested OS: Ubuntu 17.04, 16.04, 14.04 & Linux-Mint-18
#TODO: remove dependency on sudo user account to run script (i.e. run as root and specifiy LUX user so LUX user does not require sudo privileges)
#TODO: add specific dependencies depending on build option (i.e. gui requires QT5)

noflags() {
	echo "┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄"
    echo "Usage: Auto configure and build LUX wallet for linux"
    echo "Example: Auto configure"
    echo "┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄"
    exit 1
}

message() {
	echo "╒════════════════════════════════════════════════════════════════════════════════>>"
	echo "| $1"
	echo "╘════════════════════════════════════════════<<<"
}

error() {
	message "An error occured, you must fix it to continue!"
	exit 1
}

prepdependencies() { #TODO: add error detection
	message "Installing LUX dependencies..."
	sudo apt-get update
	sudo apt-get install -y qt4-qmake libqt4-dev libminiupnpc-dev libdb++-dev libdb-dev libcrypto++-dev libqrencode-dev libboost-all-dev build-essential libboost-system-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev libssl-dev libdb++-dev libssl-dev ufw git software-properties-common build-essential libtool autotools-dev autoconf pkg-config libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev automake g++-mingw-w64-x86-64
	sudo add-apt-repository -y ppa:bitcoin/bitcoin
	sudo apt-get update
	sudo apt-get install -y libdb4.8-dev libdb4.8++-dev
}

clonerepo() { #TODO: add error detection
	message "Cloning LUX source from master repository..."
  	cd ~/
	git clone https://github.com/216k155/lux.git
}

compile() {
	cd lux #TODO: squash relative path
#	message "Preparing HOST mingw32 to build ...this may take a few minutes..."
#	cd depends && make HOST=x86_64-w64-mingw32
#	if [ $? -ne 0 ]; then error; fi
#   message "Install libdb4.8 (Berkeley DB4)"
#   sudo sh install-db4.sh db4 
#	if [ $? -ne 0 ]; then error; fi
#   cd ..
    if [ $? -ne 0 ]; then error; fi
	message "Build LUX...this may take a few minutes..."
	./autogen.sh
    if [ $? -ne 0 ]; then error; fi
    ./configure --disable-tests
    if [ $? -ne 0 ]; then error; fi
    make -j4
	if [ $? -ne 0 ]; then error; fi
}

createconf() {
	#TODO: Can check for flag and skip this
	#TODO: Random generate LUX user/password

	message "Creating lux.conf..."
	CONFDIR=~/.lux
	CONFILE=$CONFDIR/lux.conf
	if [ ! -d "$CONFDIR" ]; then mkdir $CONFDIR; fi
	if [ $? -ne 0 ]; then error; fi	
	mnip=$(curl -s https://api.ipify.org)
	rpcuser=$(date +%s | sha256sum | base64 | head -c 10 ; echo)
	rpcpass=$(openssl rand -base64 32)
	printf "%s\n" "rpcuser=$rpcuser" "rpcpassword=$rpcpass" "rpcallowip=127.0.0.1" "port=28666" "listen=1" "server=1" "daemon=1" "maxconnections=256" "rpcport=9888" > $CONFILE
}

success() {
	luxd
	message "SUCCESS! LUX wallet - lux.conf setting below..."
	message "LUX $mnip:28666 $MNPRIVKEY TXHASH INDEX"
	exit 0
}

install() {
	prepdependencies
	clonerepo
	compile $1
	createconf
	success
}

#main
#default to --without-gui
install --without-gui
