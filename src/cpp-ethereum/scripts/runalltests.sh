#!/usr/bin/env bash

workdir=$(pwd)

#Clean the previous build
rm -rf cpp-cpp-ethereum 2>/dev/null || true
rm -rf tests 2>/dev/null || true
exec &> $workdir/buildlog.txt
export ETHEREUM_TEST_PATH="$(pwd)/tests"

#Clonning Repositories
echo "Cloning Repositories"
git clone --depth 1 --single-branch https://github.com/cpp-ethereum/tests.git
git clone --recursive --depth 1 --single-branch https://github.com/cpp-ethereum/cpp-cpp-ethereum.git
cd tests
testHead=$(git rev-parse HEAD)
cd ..
cd cpp-cpp-ethereum
cppHead=$(git rev-parse HEAD)

#Prepare test results
mkdir build
cd build
echo "Make cpp-cpp-ethereum develop:"
cmake .. -DCOVERAGE=On
make -j8
echo "Running all tests:"
echo "cpp-cpp-ethereum repository at commit $cppHead"
echo "tests repository at commit $testHead"
exec 2> $workdir/testlog.txt
timestart=$(date +%s.%N)
test/testeth -- --all --exectimelog
timeend=$(date +%s.%N)
date=$(date +%Y-%m-%d)

# Upload coverage report
if [ -z "$CODECOV_TOKEN" ]; then
    echo "Warning! CODECOV_TOKEN not set. See https://codecov.io/gh/cpp-ethereum/cpp-cpp-ethereum/settings."
else
    bash <(curl -s https://codecov.io/bash) -n alltests -b "$date" -F alltests -a '>/dev/null 2>&1'
fi

# Make report
cd $workdir
(
echo "REPORT"
exectime=$(echo "$timeend - $timestart" | bc)
echo "Test execution time: $exectime s"
echo "Coverage: https://codecov.io/gh/cpp-ethereum/cpp-cpp-ethereum/commit/$cppHead"
cat testlog.txt
cat buildlog.txt
) > report.txt

# Send mail
RECIPIENTS="dimitry@cpp-ethereum.org pawel@cpp-ethereum.org chris@cpp-ethereum.org andrei@cpp-ethereum.org"
mail < report.txt -s "cpp-cpp-ethereum alltests $date" $RECIPIENTS
