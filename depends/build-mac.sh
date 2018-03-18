# /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
echo "Please install homebrew first too"
brew install autoconf automake pkg-config libtool librsvg cryptopp
brew install autoconf automake berkeley-db4 libtool boost miniupnpc openssl pkg-config protobuf qt5 libzmq
brew install cmake automake berkeley-db4 leveldb libtool boost --c++11 --without-single --without-static miniupnpc openssl pkg-config protobuf qt5 libevent imagemagick --with-librsvg
export MAKEJOBS=-j4
export HOST="x86_64-apple-darwin11"
export BITCOIN_CONFIG="--enable-reduce-exports --disable-tests"
export OSX_SDK="10.11"
export GOAL="deploy"
mkdir -p depends/SDKs depends/sdk-sources
cd depends/SDKs
curl -sL https://github.com/phracker/MacOSX-SDKs/releases/download/MacOSX10.11.sdk/MacOSX10.11.sdk.tar.xz | tar xJ
cd ..
make -C depends HOST=$HOST
export TRAVIS_COMMIT_LOG=`git log --format=fuller -1`
export BASE_OUTDIR=$(pwd)/out
export OUTDIR=$BASE_OUTDIR/1/1-$HOST
export BITCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$(pwd)/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"
./autogen.sh
./configure --cache-file=config.cache $BITCOIN_CONFIG_ALL $BITCOIN_CONFIG || ( cat config.log && false)
make $GOAL || ( echo "Build failure. Verbose build follows." && make $GOAL V=1 ; false )
