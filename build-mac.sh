# /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
echo "The first argument to this shell script should be 11 or 13 based on the OS you're using to compile, 13 being high sierra."
echo "e.g. ./build-mac.sh 13"
echo "Please install homebrew first too"
if [ -z "$1" ]; then exit 1; fi
brew install autoconf automake pkg-config libtool librsvg
# echo $1
export MAKEJOBS=-j3
# export HOST=x86_64-apple-darwin13
export HOST="x86_64-apple-darwin$1"
export PACKAGES="cmake imagemagick libcap-dev librsvg2-bin libz-dev libbz2-dev libtiff-tools python-dev"
export BITCOIN_CONFIG="--enable-reduce-exports"
# export OSX_SDK="10.13"
export OSX_SDK="10.$1"
export GOAL="deploy"
mkdir -p depends/SDKs depends/sdk-sources
make -C depends HOST=$HOST $DEP_OPTS
export TRAVIS_COMMIT_LOG=`git log --format=fuller -1`
export BASE_OUTDIR=$(pwd)/out
export OUTDIR=$BASE_OUTDIR/1/1-$HOST
export BITCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$(pwd)/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"
./autogen.sh
./configure --cache-file=config.cache $BITCOIN_CONFIG_ALL $BITCOIN_CONFIG || ( cat config.log && false)
make $GOAL || ( echo "Build failure. Verbose build follows." && make $GOAL V=1 ; false )
