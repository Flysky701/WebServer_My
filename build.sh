#!/bin/bash
set -e

sudo apt-get update
sudo apt-get install -y libmysqlcppconn-dev
rm -rf build && mkdir -p build && cd build
cmake ..
make

./server