#!/usr/bin/env bash
# author: Lefteris Karapetsas <lefteris@refu.co>
#
# Just upload the generated .rb file to homebrew cpp-ethereum

echo ">>> Starting the script to upload .rb file to homebrew cpp-ethereum"
rm -rf homebrew-cpp-ethereum
git clone git@github.com:cpp-ethereum/homebrew-cpp-ethereum.git
cp webthree-umbrella/build/cpp-cpp-ethereum.rb homebrew-cpp-ethereum
cd homebrew-cpp-ethereum
git add . -u
git commit -m "update cpp-cpp-ethereum.rb"
git push origin
cd ..
rm -rf homebrew-cpp-ethereum
echo ">>> Succesfully uploaded the .rb file to homebrew cpp-ethereum"
