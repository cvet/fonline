#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ "$1" = "linux" ]; then
    echo "Install packages"
    echo "Sudo required"
    sudo apt-get -qq -y update || true
    sudo apt-get -qq -y upgrade || true
    echo "Install clang"
    sudo apt-get -qq -y install clang
    echo "Install build-essential"
    sudo apt-get -qq -y install build-essential
    echo "Install cmake"
    sudo apt-get -qq -y install cmake
    echo "Install wput"
    sudo apt-get -qq -y install wput
    echo "Install libx11-dev"
    sudo apt-get -qq -y install libx11-dev
    echo "Install freeglut3-dev"
    sudo apt-get -qq -y install freeglut3-dev
    echo "Install libssl-dev"
    sudo apt-get -qq -y install libssl-dev
    echo "Install libevent-dev"
    sudo apt-get -qq -y install libevent-dev
    echo "Install libxi-dev"
    sudo apt-get -qq -y install libxi-dev
    echo "Install curl"
    sudo apt-get -qq -y install curl
    echo "Install binutils-dev"
    sudo apt-get -qq -y install binutils-dev

elif [ "$1" = "web" ] || [ "$1" = "web-debug" ]; then
    echo "Install packages"
    echo "Sudo required"
    sudo apt-get -qq -y update || true
    sudo apt-get -qq -y upgrade || true
    echo "Install build-essential"
    sudo apt-get -qq -y install build-essential
    echo "Install cmake"
    sudo apt-get -qq -y install cmake
    echo "Install wput"
    sudo apt-get -qq -y install wput
    echo "Install python"
    sudo apt-get -qq -y install python
    echo "Install nodejs"
    sudo apt-get -qq -y install nodejs
    echo "Install default-jre"
    sudo apt-get -qq -y install default-jre
    echo "Install git-core"
    sudo apt-get -qq -y install git-core

    if [ ! -d "emsdk" ]; then
        echo "Setup Emscripten"
        mkdir -p emsdk
        cp -r "$FO_ROOT/BuildTools/emsdk" "./"
        cd emsdk
        chmod +x ./emsdk
        ./emsdk update
        ./emsdk list
        ./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
        ./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
        rm -rf releases/.git
        cd ../
    fi

elif [ "$1" = "android" ] || [ "$1" = "android-arm64" ]; then
    echo "Install packages"
    echo "Sudo required"
    sudo apt-get -qq -y update || true
    sudo apt-get -qq -y upgrade || true
    echo "Install build-essential"
    sudo apt-get -qq -y install build-essential
    echo "Install cmake"
    sudo apt-get -qq -y install cmake
    echo "Install unzip"
    sudo apt-get -qq -y install unzip
    echo "Install wput"
    sudo apt-get -qq -y install wput
    echo "Install ant"
    sudo apt-get -qq -y install ant
    echo "Install openjdk-8-jdk"
    sudo apt-get -qq -y install openjdk-8-jdk
    echo "Install python"
    sudo apt-get -qq -y install python
    echo "Install android-sdk"
    sudo apt-get -qq -y install android-sdk

    if [ ! -d "$ANDROID_NDK_VERSION" ]; then
        echo "Download Android NDK"
        wget -q "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
        echo "Unzip Android NDK"
        unzip -qq -o "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"

        echo "Generate toolchains"
        cd $ANDROID_NDK_VERSION/build/tools
        python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm-toolchain
        python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm64-toolchain
        cd ../../../
    fi
fi

$CUR_DIR/build.sh $1 $2

if [ "$2" = "unit-tests" ]; then
    echo "Run unit tests"
    $OUTPUT_PATH/Tests/FOnlineUnitTests

elif [ "$2" = "code-coverage" ]; then
    echo "Run code coverage"
    $OUTPUT_PATH/Tests/FOnlineCodeCoverage

    if [[ ! -z "$CODECOV_TOKEN" ]]; then
        echo "Upload reports to codecov.io"
        bash <(curl -s https://codecov.io/bash)
    fi
fi
