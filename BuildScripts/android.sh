#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "arm32" ] && [ ! "$1" = "arm64" ]; then
    echo "Invalid build argument, allowed only arm32 or arm64"
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
fi

mkdir -p $FO_WORKSPACE && cd $FO_WORKSPACE

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

echo "Copy placeholder"
mkdir -p "$FO_OUTPUT_PATH/Client/Android" && rm -rf "$FO_OUTPUT_PATH/Client/Android/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/android-project/." "$FO_OUTPUT_PATH/Client/Android"

if [ "$1" = "" ] || [ "$1" = "arm32" ]; then
    echo "Build Arm32 binaries"
    export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm-toolchain
    export ANDROID_ABI=armeabi-v7a
    mkdir -p "build-android-$ANDROID_ABI" && cd "build-android-$ANDROID_ABI"
    cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" "$ROOT_FULL_PATH"
    make -j$(nproc)
    cd ../
fi

if [ "$1" = "" ] || [ "$1" = "arm64" ]; then
    echo "Build Arm64 binaries"
    export ANDROID_STANDALONE_TOOLCHAIN=$FO_WORKSPACE/arm64-toolchain
    export ANDROID_ABI=arm64-v8a
    mkdir -p "build-android-$ANDROID_ABI" && cd "build-android-$ANDROID_ABI"
    cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="$FO_OUTPUT_PATH" "$ROOT_FULL_PATH"
    make -j$(nproc)
    cd ../
fi
