#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "release" ] && [ ! "$1" = "debug" ]; then
    echo "Invalid build argument, allowed only release or debug"
    exit 1
fi

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh

if [[ "$FO_INSTALL_PACKAGES" = "1" ]]; then
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
fi

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

if [ ! -d "emsdk" ]; then
    echo "Setup Emscripten"
    mkdir -p emsdk
    cp -r "$ROOT_FULL_PATH/BuildScripts/emsdk" "./"
    cd emsdk
    chmod +x ./emsdk
    ./emsdk update
    ./emsdk list
    ./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
    ./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
    rm -rf releases/.git
    cd ../
fi

cd emsdk
source ./emsdk_env.sh
cd ../
emcc -v

echo "Copy placeholder"
mkdir -p "$FO_OUTPUT_PATH/Client/Web" && rm -rf "$FO_OUTPUT_PATH/Client/Web/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/web/." "$FO_OUTPUT_PATH/Client/Web"

if [ "$1" = "" ] || [ "$1" = "release" ]; then
    echo "Build release binaries"
    mkdir -p "build-web-release" && cd "build-web-release"
    cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" -DFONLINE_WEB_DEBUG=OFF "$ROOT_FULL_PATH"
    make -j$(nproc)
    cd ../
fi

if [ "$1" = "" ] || [ "$1" = "debug" ]; then
    echo "Build debug binaries"
    mkdir -p "build-web-debug" && cd "build-web-debug"
    cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" -DFONLINE_WEB_DEBUG=ON "$ROOT_FULL_PATH"
    make -j$(nproc)
    cd ../
fi
