#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "unit-tests" ] && [ ! "$1" = "code-coverage" ]; then
    echo "Invalid build argument, allowed only unit-tests or code-coverage"
    exit 1
fi

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

if [[ "$FO_INSTALL_PACKAGES" = "1" ]]; then
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
fi

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

if [ "$1" = "unit-tests" ]; then
    echo "Build unit tests binaries"
    mkdir -p "build-linux-unit-tests" && cd "build-linux-unit-tests"
    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" -DFONLINE_UNIT_TESTS=1 -DFONLINE_BUILD_CLIENT=0 "$ROOT_FULL_PATH"
elif [ "$1" = "code-coverage" ]; then
    echo "Build code coverage binaries"
    mkdir -p "build-linux-code-coverage" && cd "build-linux-code-coverage"
    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" -DFONLINE_CODE_COVERAGE=1 -DFONLINE_BUILD_CLIENT=0 "$ROOT_FULL_PATH"
else
    echo "Build default binaries"
    mkdir -p "build-linux" && cd "build-linux"
    cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "$ROOT_FULL_PATH"
fi

make -j$(nproc)
