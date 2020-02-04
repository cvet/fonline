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

# Cross compilation using OSXCross
# https://github.com/tpoechtrager/osxcross
# Toolchain must be installed in home dir
if [ -d "$HOME/osxcross" ]; then
    echo "OSXCross cross compilation"
    export PATH=$PATH:$HOME/osxcross/target/bin
    CMAKE_GEN="$HOME/osxcross/target/bin/x86_64-apple-darwin19-cmake -G \"Unix Makefiles\" -DCMAKE_TOOLCHAIN_FILE=\"$HOME/osxcross/tools/toolchain.cmake\""
else
    CMAKE_GEN="$CMAKE -G \"Xcode\""
fi

mkdir -p $FO_BUILD_DEST && cd $FO_BUILD_DEST
mkdir -p iOS && cd iOS

echo "Build default binaries"
mkdir -p default && cd default
eval $CMAKE_GEN -C "$ROOT_FULL_PATH/BuildScripts/ios.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
$CMAKE --build . --config Release --target FOnline
