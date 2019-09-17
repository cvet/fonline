#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [ -x "$(command -v cmake)" ]; then
    CMAKE=cmake
else
    CMAKE=/Applications/CMake.app/Contents/bin/cmake
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

mkdir -p macOS
cd macOS
$CMAKE -G Xcode -DFONLINE_OUTPUT_BINARIES_PATH="../" "$ROOT_FULL_PATH"
$CMAKE --build . --config RelWithDebInfo --target FOnline
cd ../
