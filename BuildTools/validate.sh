#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ "$1" = "linux-client" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-client" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "android-arm-client" ]; then
    TARGET=android
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "android-arm64-client" ]; then
    TARGET=android-arm64
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "android-x86-client" ]; then
    TARGET=android-x86
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "web-client" ]; then
    TARGET=web
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "mac-client" ]; then
    TARGET=mac
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$1" = "ios-client" ]; then
    TARGET=ios
    BUILD_TARGET="-DFONLINE_BUILD_CLIENT=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$1" = "linux-server" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_SERVER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-server" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_SERVER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "linux-single" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-single" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "android-arm-single" ]; then
    TARGET=android
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "android-arm64-single" ]; then
    TARGET=android-arm64
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "android-x86-single" ]; then
    TARGET=android-x86
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "web-single" ]; then
    TARGET=web
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "mac-single" ]; then
    TARGET=mac
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$1" = "ios-single" ]; then
    TARGET=ios
    BUILD_TARGET="-DFONLINE_BUILD_SINGLE=1 -DFONLINE_UNIT_TESTS=0"
elif [ "$1" = "linux-mapper" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_MAPPER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-mapper" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_MAPPER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "linux-ascompiler" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-ascompiler" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "linux-baker" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_BAKER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "linux-gcc-baker" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_BUILD_BAKER=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
    USE_GCC=1
elif [ "$1" = "unit-tests" ]; then
    TARGET=linux
    BUILD_TARGET="-DFONLINE_UNIT_TESTS=1 -DCMAKE_BUILD_TYPE=Debug"
elif [ "$1" = "code-coverage" ]; then
    TARGET=linux
    USE_GCC=1
    BUILD_TARGET="-DFONLINE_CODE_COVERAGE=1 -DFONLINE_UNIT_TESTS=0 -DCMAKE_BUILD_TYPE=Debug"
else
    echo "Invalid argument"
    exit 1
fi

if [ "$TARGET" != "mac" ] && [ "$TARGET" != "ios" ]; then
    $CUR_DIR/prepare-workspace.sh $TARGET
fi

BUILD_DIR=validate-$1
mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ "$TARGET" = "linux" ]; then
	if [ "$USE_GCC" = "1" ]; then
		export CC=/usr/bin/gcc
		export CXX=/usr/bin/g++
	else
		export CC=/usr/bin/clang
		export CXX=/usr/bin/clang++
	fi

    cmake -G "Unix Makefiles" $BUILD_TARGET "$FO_ROOT"
    cmake --build . --config Debug

elif [ "$TARGET" = "web" ]; then
    source $FO_WORKSPACE/emsdk/emsdk_env.sh

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" $BUILD_TARGET "$FO_ROOT"
    cmake --build . --config Debug

elif [ "$TARGET" = "android" ] || [ "$TARGET" = "android-arm64" ] || [ "$TARGET" = "android-x86" ]; then
    export ANDROID_NDK=$FO_WORKSPACE/android-ndk

    if [ "$TARGET" = "android" ]; then
        export ANDROID_ABI=armeabi-v7a
    elif [ "$TARGET" = "android-arm64" ]; then
        export ANDROID_ABI=arm64-v8a
    elif [ "$TARGET" = "android-x86" ]; then
        export ANDROID_ABI=x86
    fi

    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/android.cache.cmake" $BUILD_TARGET "$FO_ROOT"
    cmake --build . --config Debug

elif [ "$TARGET" = "mac" ] || [ "$TARGET" = "ios" ]; then
    if [ -x "$(command -v cmake)" ]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi

    if [ "$TARGET" = "mac" ]; then
        $CMAKE -G "Xcode" $BUILD_TARGET "$FO_ROOT"
    else
        $CMAKE -G "Xcode" -C "$FO_ROOT/BuildTools/ios.cache.cmake" $BUILD_TARGET "$FO_ROOT"
    fi

    $CMAKE --build . --config Debug
fi

if [ "$1" = "unit-tests" ]; then
    echo "Run unit tests"
    ./Tests/FOnlineUnitTests

elif [ "$1" = "code-coverage" ]; then
    echo "Run code coverage"
    ./Tests/FOnlineCodeCoverage

    if [[ ! -z "$CODECOV_TOKEN" ]]; then
        echo "Upload reports to codecov.io"
        rm -f codecov.sh
        curl -s https://codecov.io/bash > codecov.sh
        chmod +x codecov.sh
        ./codecov.sh -Z -s "$(cd .; pwd)"
    fi
fi
