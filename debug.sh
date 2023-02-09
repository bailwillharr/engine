#!/bin/sh
set -e
cd Debug
cmake --build .
cd test
./enginetest $@
