#!/bin/bash -e

if [ ! "$1" = "" ]; then
	echo "Invalid build argument, not allowed"
	exit 1
fi

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

echo "Setup environment"
export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

echo "Find cmake"
if [ -x "$(command -v cmake)" ]; then
    CMAKE=cmake
else
    CMAKE=/Applications/CMake.app/Contents/bin/cmake
fi

mkdir -p $FO_BUILD_DEST && cd $FO_BUILD_DEST
mkdir -p macOS && cd macOS

echo "Build default binaries"
mkdir -p default && cd default
$CMAKE -G Xcode -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
$CMAKE --build . --config RelWithDebInfo --target FOnline
