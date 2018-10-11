#!/bin/bash

cd src
make

cd ../jni
make

cd ../cpp
make

cd ../node
npm install

cd ../examples/linux
make

cd ../java
make

cd ../cpp/linux
make
