sudo apt-get install curl librsvg2-bin libtiff-tools bsdmainutils cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools
sudo easy_install argparse
export HOST="x86_64-apple-darwin11"
export BITCOIN_CONFIG="--disable-tests --disable-zmq"
export OSX_SDK="10.11"
mkdir -p depends/SDKs depends/sdk-sources
cd depends/SDKs
curl -sL https://github.com/216k155/MacOSX-SDKs/releases/download/MacOSX10.11.sdk/MacOSX10.11.sdk.tar.xz | tar xJ
cd ../..
make -C depends HOST=$HOST
export BASE_OUTDIR=$(pwd)/out
export OUTDIR=$BASE_OUTDIR/1/1-$HOST
export BITCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$(pwd)/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-apple-darwin11/share/config.site
./configure --host="x86_64-apple-darwin11" --prefix=$(pwd)/depends/x86_64-apple-darwin11 --disable-tests --disable-zmq
make HOST=x86_64-apple-darwin11 -j$(nproc)
make deploy
