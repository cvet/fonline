#!/bin/bash -e

if [[ -z $1 || -z $2 ]]; then
    echo "Provide at least two arguments"
    exit 1
fi

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/internal-tools.sh

mkdir -p $FO_WORKSPACE
mkdir -p $FO_OUTPUT
pushd $FO_WORKSPACE

if [[ $2 = "client" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=1 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "server" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=1 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "editor" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=1 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "mapper" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=1 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "ascompiler" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "baker" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "unit-tests" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=1 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "code-coverage" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=0 -DFO_BUILD_BAKER=0 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=1"
elif [[ $2 = "toolset" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=0 -DFO_BUILD_SERVER=0 -DFO_BUILD_EDITOR=0 -DFO_BUILD_MAPPER=0 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
elif [[ $2 = "full" ]]; then
    BUILD_TARGET="-DFO_BUILD_CLIENT=1 -DFO_BUILD_SERVER=1 -DFO_BUILD_EDITOR=1 -DFO_BUILD_MAPPER=1 -DFO_BUILD_ASCOMPILER=1 -DFO_BUILD_BAKER=1 -DFO_UNIT_TESTS=0 -DFO_CODE_COVERAGE=0"
else
    echo "Invalid second command arg"
    exit 1
fi

if [[ -z $3 ]]; then
    CONFIG="Release"
else
    CONFIG="$3"
fi

BUILD_DIR="build-$1-$2-$CONFIG"

mkdir -p $BUILD_DIR
cd $BUILD_DIR

rm -rf "READY"

if [[ $1 = "linux" ]]; then
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    cmake -G "Unix Makefiles" -DFO_OUTPUT_PATH="$FO_OUTPUT" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG "$FO_PROJECT_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [[ $1 = "web" ]]; then
    source $FO_WORKSPACE/emsdk/emsdk_env.sh

    cmake -G "Unix Makefiles" -C "$FO_ENGINE_ROOT/BuildTools/web.cache.cmake" -DFO_OUTPUT_PATH="$FO_OUTPUT" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG "$FO_PROJECT_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [[ $1 = "android" || $1 = "android-arm64" || $1 = "android-x86" ]]; then
    if [[ $1 = "android" ]]; then
        export ANDROID_ABI=armeabi-v7a
    elif [[ $1 = "android-arm64" ]]; then
        export ANDROID_ABI=arm64-v8a
    elif [[ $1 = "android-x86" ]]; then
        export ANDROID_ABI=x86
    fi

    cmake -G "Unix Makefiles" -C "$FO_ENGINE_ROOT/BuildTools/android.cache.cmake" -DFO_OUTPUT_PATH="$FO_OUTPUT" $BUILD_TARGET -DCMAKE_BUILD_TYPE=$CONFIG "$FO_PROJECT_ROOT"
    cmake --build . --config $CONFIG --parallel

elif [[ $1 = "mac" || $1 = "ios" ]]; then
    if [[ -x $(command -v cmake) ]]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi

    if [[ $1 = "mac" ]]; then
        $CMAKE -G "Xcode" -DFO_OUTPUT_PATH="$FO_OUTPUT" $BUILD_TARGET "$FO_PROJECT_ROOT"
        $CMAKE --build . --config $CONFIG --parallel
    else
        $CMAKE -G "Xcode" -C "$FO_ENGINE_ROOT/BuildTools/ios.cache.cmake" -DFO_OUTPUT_PATH="$FO_OUTPUT" $BUILD_TARGET "$FO_PROJECT_ROOT"
        $CMAKE --build . --config $CONFIG --parallel
    fi

else
    echo "Invalid first command arg"
    exit 1
fi

touch "READY"
popd
