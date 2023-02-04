#!/bin/sh
set -e
mkdir -p Debug
cd Debug
cmake -G Ninja -D CMAKE_BUILD_TYPE=Debug -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
cd ..
ln -sf Debug/compile_commands.json .
mkdir -p Release
cd Release
cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
