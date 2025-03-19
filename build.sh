#!/bin/bash
set -e

sudo apt-get update
sudo apt-get install -y libmysqlcppconn-dev
sudo apt-get install -y libmysqlclient-dev
rm -rf build && mkdir -p build && cd build
cmake ..
make

./server