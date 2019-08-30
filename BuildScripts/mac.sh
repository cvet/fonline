#!/bin/bash -ex

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p mac
cd mac
mkdir -p Mac
rm -rf Mac/*

pwd

/Applications/CMake.app/Contents/bin/cmake -G Xcode "$ROOT_FULL_PATH"
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline
