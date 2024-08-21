#!/bin/bash

set -e

# sudo apt-get install libdb5.3++-dev libjemalloc-dev libmysqld-dev libaio-dev

git submodule update --init --recursive

pushd silo
git apply ../silo.patch || true
make dbtest -j
popd
