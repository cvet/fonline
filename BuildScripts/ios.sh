#!/bin/bash -ex

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p ios
cd ios
mkdir -p iOS
rm -rf iOS/*

pwd

/Applications/CMake.app/Contents/bin/cmake -C "$SOURCE_FULL_PATH/BuildScripts/ios.cache.cmake" "$SOURCE_FULL_PATH/Source"
make
