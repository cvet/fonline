#!/bin/bash

cd src
make clean

cd ../jni
make clean

cd ../cpp
make clean

cd ../node
rm -r build
rm -r node_modules

cd ../examples/linux
make clean

cd ../java
make clean

cd ../cpp/linux
make clean
