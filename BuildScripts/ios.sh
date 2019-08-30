#!/bin/bash -ex

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p ios
cd ios
mkdir -p iOS
rm -rf iOS/*

pwd

/Applications/CMake.app/Contents/bin/cmake -C "$ROOT_FULL_PATH/BuildScripts/ios.cache.cmake" "$ROOT_FULL_PATH"
/Applications/CMake.app/Contents/bin/cmake --build . --config Release --target FOnline
