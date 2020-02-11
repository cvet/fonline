#!/bin/bash -e

echo "Sudo required"
sudo -v

echo "Prepare workspace"

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

if [ -f "$FO_WORKSPACE/workspace-version.txt" ]; then
    VER=`cat $FO_WORKSPACE/workspace-version.txt`
    if [[ "$VER" = "$FO_WORKSPACE_VERSION" ]]; then
        echo "Workspace is actual"
        exit
    fi
fi

if [ -d "$FO_WORKSPACE" ]; then
    echo "Remove previous installation"
    rm -rf $FO_WORKSPACE
fi

echo "Update packages"
sudo apt-get -qq -y update

echo "Upgrade packages"
sudo apt-get -qq -y upgrade

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
echo "Install git"
sudo apt-get -qq -y install git
echo "Install clang"
sudo apt-get -qq -y install clang
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
echo "Install unzip"
sudo apt-get -qq -y install unzip
echo "Install ant"
sudo apt-get -qq -y install ant
echo "Install openjdk-8-jdk"
sudo apt-get -qq -y install openjdk-8-jdk
echo "Install patch"
sudo apt-get -qq -y install patch
echo "Install lzma-dev"
sudo apt-get -qq -y install lzma-dev
echo "Install libxml2-dev"
sudo apt-get -qq -y install libxml2-dev
echo "Install llvm-dev"
sudo apt-get -qq -y install llvm-dev
echo "Install uuid-dev"
sudo apt-get -qq -y install uuid-dev
echo "Install android-sdk"
sudo apt-get -qq -y install android-sdk

mkdir $FO_WORKSPACE && cd $FO_WORKSPACE

function setup_osxcross()
{
    echo "Setup OSXCross"
    git clone --depth 1 https://github.com/tpoechtrager/osxcross
    cd osxcross
    rm -rf .git
    cp "$ROOT_FULL_PATH/BuildScripts/osxcross/MacOSX10.15.sdk.tar.bz2" "./tarballs"
    export UNATTENDED=1
    ./build.sh
}

function setup_emscripten()
{
    echo "Setup Emscripten"
    mkdir emsdk
    cp -r "$ROOT_FULL_PATH/BuildScripts/emsdk" "./"
    cd emsdk
    chmod +x ./emsdk
    ./emsdk update
    ./emsdk list
    ./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
    ./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
    rm -rf releases/.git
}

function setup_android_ndk()
{
    echo "Download Android NDK"
    wget -qq "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
    echo "Unzip Android NDK"
    unzip -qq -o "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
    rm -f "$ANDROID_NDK_VERSION-linux-x86_64.zip"

    echo "Generate Android toolchains"
    cd $ANDROID_NDK_VERSION/build/tools
    python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm-toolchain
    python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm64-toolchain
}

run_job setup_osxcross
run_job setup_emscripten
run_job setup_android_ndk
wait_jobs

echo $FO_WORKSPACE_VERSION > "workspace-version.txt"
echo "Workspace is ready"
