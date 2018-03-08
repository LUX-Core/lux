#!/bin/sh
set -e -x

sudo apt-get -qq install -y doxygen
doxygen docs/Doxyfile
git add docs
git commit -m "Update docs"
git push -f "https://$GITHUB_TOKEN@github.com/cpp-ethereum/evmjit.git" HEAD:gh-pages
