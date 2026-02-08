#!/bin/bash
if [[ $1 == "clean" ]]; then
rm -rf build
mkdir build
fi
cd build
cmake ..
cmake --build .

