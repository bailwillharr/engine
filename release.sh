#!/bin/sh
set -e
cd Release
cmake --build .
cd test
./enginetest $@
