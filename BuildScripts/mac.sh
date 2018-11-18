#!/bin/bash -ex

[ "$FO_ROOT" ] || { echo "FO_ROOT variable is not set"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST variable is not set"; exit 1; }

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p mac
cd mac
mkdir -p Mac
rm -rf Mac/*

pwd

/Applications/CMake.app/Contents/bin/cmake -G Xcode "$ROOT_FULL_PATH/Source"
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline
