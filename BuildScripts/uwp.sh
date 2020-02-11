#!/bin/bash -e

if [ ! "$1" = "" ]; then
    echo "Invalid build argument, not allowed"
    exit 1
fi

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

FO_OUTPUT_PATH_WIN=`wsl_path_to_windows "$FO_OUTPUT_PATH"`
ROOT_FULL_PATH_WIN=`wsl_path_to_windows "$ROOT_FULL_PATH"`

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

echo "Build Win64 binaries"
mkdir -p "build-uwp-x64" && cd "build-uwp-x64"
cmake.exe -G "Visual Studio 16 2019" -A x64 -C "$ROOT_FULL_PATH_WIN/BuildScripts/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH_WIN" "$ROOT_FULL_PATH_WIN"
cmake.exe --build . --config RelWithDebInfo
