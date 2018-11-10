#!/bin/bash -ex

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p mac
cd mac
mkdir -p Mac
rm -rf Mac/*

pwd

/Applications/CMake.app/Contents/bin/cmake -G Xcode "$SOURCE_FULL_PATH/Source"
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline
