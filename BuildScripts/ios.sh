#!/bin/bash -ex

[ "$FO_ROOT" ] || { echo "FO_ROOT variable is not set"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST variable is not set"; exit 1; }

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p ios
cd ios
mkdir -p iOS
rm -rf iOS/*

pwd

/Applications/CMake.app/Contents/bin/cmake -C "$ROOT_FULL_PATH/BuildScripts/ios.cache.cmake" "$ROOT_FULL_PATH/Source"
/Applications/CMake.app/Contents/bin/cmake --build . --config Release --target FOnline
