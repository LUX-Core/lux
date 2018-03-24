# Remove any old qt5 lingering around
brew uninstall qt5

# Make sure we have the C libs we need
brew install boost@1.60 pkg-config automake

# Install some deps
brew install qt@5.5 protobuf miniupnpc openssl qrencode berkeley-db4 zlib libevent

# Make sure our stuff is linked in our path
brew link automake autoconf
