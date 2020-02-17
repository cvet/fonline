#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

if [ "$2" = "full" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1"
    BUILD_DIR="build-$1"
elif [ "$2" = "unit-tests" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_UNIT_TESTS=1"
    BUILD_DIR="build-$1-unit-tests"
elif [ "$2" = "code-coverage" ]; then
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=0 -DFONLINE_CODE_COVERAGE=1"
    BUILD_DIR="build-$1-code-coverage"
else
    BUILD_TARGETS="-DFONLINE_BUILD_CLIENT=1"
    BUILD_DIR="build-$1"
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ "$1" = "win64" ] || [ "$1" = "win32" ] || [ "$1" = "uwp" ]; then
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    OUTPUT_PATH_WIN=`wsl_path_to_windows "$OUTPUT_PATH"`

    if [ "$1" = "win64" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    elif [ "$1" = "win32" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    elif [ "$1" = "uwp" ]; then
        cmake.exe -G "Visual Studio 16 2019" -A x64 -C "$FO_ROOT_WIN/BuildTools/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" $BUILD_TARGETS "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    fi

elif [ "$1" = "linux" ]; then
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
    cmake --build . --config RelWithDebInfo

elif [ "$1" = "web" ] || [ "$1" = "web-debug" ]; then
    source $FO_WORKSPACE/emsdk/emsdk_env.sh

    if [ "$1" = "web" ]; then
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        cmake --build . --config Release
    elif [ "$1" = "web-debug" ]; then
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_WEB_DEBUG=ON -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        cmake --build . --config Debug
    fi

elif [ "$1" = "android" ] || [ "$1" = "android-arm64" ]; then
    if [ [ "$1" = "android" ]; then
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm-toolchain
        export ANDROID_ABI=armeabi-v7a
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        cmake --build . --config Release
    elif [ "$1" = "android-arm64" ]; then
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm64-toolchain
        export ANDROID_ABI=arm64-v8a
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        cmake --build . --config Release
    fi

elif [ "$1" = "mac" ] || [ "$1" = "ios" ]; then
    if [ -x "$(command -v cmake)" ]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi

    # Cross compilation using OSXCross
    if [ -d "$FO_WORKSPACE/osxcross" ]; then
        echo "OSXCross cross compilation"
        export OSXCROSS_DIR=$(cd $FO_WORKSPACE/osxcross; pwd)
        export PATH=$PATH:$OSXCROSS_DIR/target/bin
        CMAKE_GEN="$OSXCROSS_DIR/target/bin/x86_64-apple-darwin19-cmake -G \"Unix Makefiles\" -DCMAKE_TOOLCHAIN_FILE=\"$OSXCROSS_DIR/tools/toolchain.cmake\""
    else
        CMAKE_GEN="$CMAKE -G \"Xcode\""
    fi

    if [ "$1" = "mac" ]; then
        eval $CMAKE_GEN -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        $CMAKE --build . --config RelWithDebInfo --target FOnline
    elif [ "$1" = "ios" ]; then
        eval $CMAKE_GEN -C "$FO_ROOT/BuildTools/ios.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" $BUILD_TARGETS "$FO_ROOT"
        $CMAKE --build . --config Release --target FOnline
    fi
fi
