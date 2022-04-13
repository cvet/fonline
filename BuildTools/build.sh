#!/bin/bash -e

if [ "$1" = "" ] || [ "$2" = "" ]; then
    echo "Provide at least two arguments"
    exit 1
fi

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

if [ "$2" = "client" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "server" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_SERVER=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "single" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "mapper" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_MAPPER=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "ascompiler" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "baker" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_BAKER=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "unit-tests" ]; then
    BUILD_TARGET="-DFONLINE_UNIT_TESTS=1"
elif [ "$2" = "code-coverage" ]; then
    BUILD_TARGET="--DFONLINE_CODE_COVERAGE=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "toolset" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_BAKER=1 -DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$2" = "full" ]; then
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_SINGLE=1 -DFONLINE_BUILD_MAPPER=1 -DFONLINE_BUILD_BAKER=1 -DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0"
else
    echo "Invalid second command arg"
    exit 1
fi

BUILD_DIR="build-$1-$2"
CONFIG="Release"
if [ "$3" = "debug" ]; then
    CONFIG="Debug"
    BUILD_DIR="$BUILD_DIR-debug"
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

rm -rf ready

OUTPUT_PATH=$FO_WORKSPACE/output

if [ "$1" = "linux" ]; then
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [ "$1" = "web" ]; then
    source $FO_WORKSPACE/emsdk/emsdk_env.sh

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [ "$1" = "android" ] || [ "$1" = "android-arm64" ] || [ "$1" = "android-x86" ]; then
    export ANDROID_NDK=$FO_WORKSPACE/android-ndk

    if [ "$1" = "android" ]; then
        export ANDROID_ABI=armeabi-v7a
    elif [ "$1" = "android-arm64" ]; then
        export ANDROID_ABI=arm64-v8a
    elif [ "$1" = "android-x86" ]; then
        export ANDROID_ABI=x86
    fi

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [ "$1" = "mac" ] || [ "$1" = "ios" ]; then
    if [ -x "$(command -v cmake)" ]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi

    if [ "$1" = "mac" ]; then
        $CMAKE -G "Xcode" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
        $CMAKE --build . --config $CONFIG
    else
        $CMAKE -G "Xcode" -C "$FO_ROOT/BuildTools/ios.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
        $CMAKE --build . --config $CONFIG
    fi

elif [ "$1" = "ps4" ]; then
    cmake.exe -G "Unix Makefiles" -A x64 -C "$FO_ROOT/BuildTools/ps4.cache.cmake" -DFONLINE_OUTPUT_PATH="$OUTPUT_PATH" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake.exe --build . --config $CONFIG --parallel

else
    echo "Invalid first command arg"
    exit 1
fi

touch ready
