#!/bin/bash -e

if [ ! "$1" = "" ]; then
    echo "Invalid build argument, not allowed"
    exit 1
fi

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

FO_OUTPUT_WIN=`wsl_path_to_windows "$FO_OUTPUT"`
FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

echo "Build Win64 binaries"
mkdir -p "build-uwp-x64" && cd "build-uwp-x64"
cmake.exe -G "Visual Studio 16 2019" -A x64 -C "$FO_ROOT_WIN/BuildScripts/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_WIN" "$FO_ROOT_WIN"
cmake.exe --build . --config RelWithDebInfo
