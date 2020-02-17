#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

if [ "$1" = "win64" ] || [ "$1" = "win32" ] || [ "$1" = "uwp" ]; then
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    OUTPUT_PATH_WIN=`wsl_path_to_windows "$OUTPUT_PATH"`

    if [ "$1" = "win64" ]; then
        echo "Build Win64 binaries"
        mkdir -p "build-win64" && cd "build-win64"
        cmake.exe -G "Visual Studio 16 2019" -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    elif [ "$1" = "win32" ]; then
        echo "Build Win32 binaries"
        mkdir -p "build-win32" && cd "build-win32"
        cmake.exe -G "Visual Studio 16 2019" -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    elif [ "$1" = "uwp" ]; then
        echo "Build UWP binaries"
        mkdir -p "build-uwp" && cd "build-uwp"
        cmake.exe -G "Visual Studio 16 2019" -A x64 -C "$FO_ROOT_WIN/BuildTools/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH_WIN" "$FO_ROOT_WIN"
        cmake.exe --build . --config RelWithDebInfo
    fi

elif [ "$1" = "linux" ] || [ "$1" = "unit-tests" ] || [ "$1" = "code-coverage" ]; then
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++

    if [ "$1" = "linux" ]; then
        echo "Build Linux binaries"
        mkdir -p "build-linux" && cd "build-linux"
        cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "$FO_ROOT"
        cmake --build . --config RelWithDebInfo
    elif [ "$1" = "unit-tests" ]; then
        echo "Build unit tests binaries"
        mkdir -p "build-unit-tests" && cd "build-unit-tests"
        cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" -DFONLINE_UNIT_TESTS=1 -DFONLINE_BUILD_CLIENT=0 "$FO_ROOT"
        cmake --build . --config RelWithDebInfo
    elif [ "$1" = "code-coverage" ]; then
        echo "Build code coverage binaries"
        mkdir -p "build-code-coverage" && cd "build-code-coverage"
        cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" -DFONLINE_CODE_COVERAGE=1 -DFONLINE_BUILD_CLIENT=0 "$FO_ROOT"
        cmake --build . --config RelWithDebInfo
    fi

elif [ "$1" = "web" ] || [ "$1" = "web-debug" ]; then
    source ./emsdk/emsdk_env.sh

    if [ "$1" = "web" ]; then
        echo "Build Web release binaries"
        mkdir -p "build-web-release" && cd "build-web-release"
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" "$FO_ROOT"
        cmake --build . --config Release
    elif [ "$1" = "web-debug" ]; then
        echo "Build Web debug binaries"
        mkdir -p "build-web-debug" && cd "build-web-debug"
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" -DFONLINE_WEB_DEBUG=ON "$FO_ROOT"
        cmake --build . --config Debug
    fi

elif [ "$1" = "android" ] || [ "$1" = "android-arm64" ]; then
    if [ [ "$1" = "android" ]; then
        echo "Build Android arm binaries"
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm-toolchain
        export ANDROID_ABI=armeabi-v7a
        mkdir -p "build-android-$ANDROID_ABI" && cd "build-android-$ANDROID_ABI"
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" "$FO_ROOT"
        cmake --build . --config Release
    elif [ "$1" = "android-arm64" ]; then
        echo "Build Android arm64 binaries"
        export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm64-toolchain
        export ANDROID_ABI=arm64-v8a
        mkdir -p "build-android-$ANDROID_ABI" && cd "build-android-$ANDROID_ABI"
        cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" "$FO_ROOT"
        cmake --build . --config Release
    fi

elif [ "$1" = "mac" ] || [ "$1" = "ios" ]; then
    if [ -x "$(command -v cmake)" ]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi

    # Cross compilation using OSXCross
    if [ -d "osxcross" ]; then
        echo "OSXCross cross compilation"
        export OSXCROSS_DIR=$(cd osxcross; pwd)
        export PATH=$PATH:$OSXCROSS_DIR/target/bin
        CMAKE_GEN="$OSXCROSS_DIR/target/bin/x86_64-apple-darwin19-cmake -G \"Unix Makefiles\" -DCMAKE_TOOLCHAIN_FILE=\"$OSXCROSS_DIR/tools/toolchain.cmake\""
    else
        CMAKE_GEN="$CMAKE -G \"Xcode\""
    fi

    if [ "$1" = "mac" ]; then
        echo "Build macOS binaries"
        mkdir -p "build-macOS" && cd "build-macOS"
        eval $CMAKE_GEN -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" "$FO_ROOT"
        $CMAKE --build . --config RelWithDebInfo --target FOnline
    elif [ "$1" = "ios" ]; then
        echo "Build iOS binaries"
        mkdir -p "build-iOS" && cd "build-iOS"
        eval $CMAKE_GEN -C "$FO_ROOT/BuildTools/ios.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$OUTPUT_PATH" "$FO_ROOT"
        $CMAKE --build . --config Release --target FOnline
    fi
fi
