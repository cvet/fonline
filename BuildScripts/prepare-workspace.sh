#!/bin/bash -e

echo "Prepare workspace"

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh

if [ -f "$FO_BUILD_DEST/_envv" ]; then
    VER=`cat $FO_BUILD_DEST/_envv`
    if [[ "$VER" = "$FO_ENV_VERSION" ]]; then
        echo "Workspace is actual"
        exit
    fi
fi

echo "Sudo required"
sudo -v

if [ -d "$FO_BUILD_DEST" ]; then
    echo "Remove previous installation"
    rm -rf $FO_BUILD_DEST
fi

echo "Install packages"
sudo apt-get -qq -y update
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

mkdir $FO_BUILD_DEST && cd $FO_BUILD_DEST

# echo "Setup OSXCross"
# mkdir macOS && cd macOS
# git clone https://github.com/tpoechtrager/osxcross
# cd osxcross
# rm -rf .git
# copy MacOSX10.15.sdk.tar.bz2 to tarballs
# ./build.sh
# cd ../ # osxcross
# cd ../ # macOS
# mkdir iOS && cd iOS
# copy macOS osxcross to iOS
# cd ../ # iOS

echo "Setup Emscripten"
mkdir Web && cd Web
mkdir emsdk
cp -r "$ROOT_FULL_PATH/BuildScripts/emsdk" "./"
cd emsdk
chmod +x ./emsdk
./emsdk update
./emsdk list
./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
rm -rf releases/.git
cd ../ # emsdk
cd ../ # Web

echo "Download Android NDK"
mkdir Android && cd Android
wget -qq "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
echo "Unzip Android NDK"
unzip -qq -o "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"

echo "Download Android SDK"
wget -qq "https://dl.google.com/android/repository/$ANDROID_SDK_VERSION-linux.zip"
echo "Unzip Android SDK"
mkdir sdk
unzip -qq -o "$ANDROID_SDK_VERSION-linux.zip" -d "./sdk"
echo "Update Android SDK"
cd sdk && cd tools
( while sleep 3; do echo "y"; done ) | ./android update sdk --no-ui
cd ../ # tools
cd ../ # sdk

echo "Generate Android toolchains"
cd $ANDROID_NDK_VERSION/build/tools
python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm-toolchain
python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm64-toolchain
cd ../../../
cd ../ # Android

echo $FO_ENV_VERSION > _envv
echo "Workspace is ready"
