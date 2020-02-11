#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "win32" ] && [ ! "$1" = "win64" ]; then
    echo "Invalid build argument, allowed only win32 or win64"
    exit 1
fi

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

FO_OUTPUT_PATH_WIN=`wsl_path_to_windows "$FO_OUTPUT_PATH"`
ROOT_FULL_PATH_WIN=`wsl_path_to_windows "$ROOT_FULL_PATH"`

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

if [ "$1" = "win32" ]; then
    echo "Build Win32 binaries"
    mkdir -p "build-win32" && cd "build-win32"
    cmake.exe -G "Visual Studio 16 2019" -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH_WIN" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "$ROOT_FULL_PATH_WIN"
    cmake.exe --build . --config RelWithDebInfo
    cd ../
fi
if [ "$1" = "win64" ]; then
    echo "Build Win64 binaries"
    mkdir -p "build-win64" && cd "build-win64"
    cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH_WIN" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "$ROOT_FULL_PATH_WIN"
    cmake.exe --build . --config RelWithDebInfo
    cd ../
fi
