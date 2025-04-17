#!/bin/bash
set -e

sudo apt-get update
sudo apt-get install -y libmysqlcppconn-dev libmysqlclient-dev wget

mkdir -p include/nlohmann
mkdir -p build

# 下载单文件版JSON库（约1.1MB）
if [ ! -f include/nlohmann/json.hpp ]; then
    wget -q -O include/nlohmann/json.hpp \
    https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
fi

# 构建项目
cd build
cmake -DCMAKE_INCLUDE_PATH=../include ..
make -j$(nproc)

# 运行服务
./server