#!/bin/bash -ex

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

export ANDROID_NDK_VERSION="android-ndk-r12b"
export ANDROID_SDK_VERSION="tools_r25.2.3"

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	sudo apt-get -y update || true
	sudo apt-get -y install build-essential
	sudo apt-get -y install cmake
	sudo apt-get -y install unzip
	sudo apt-get -y install wput
	sudo apt-get -y install ant
	sudo apt-get -y install openjdk-8-jdk
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p android
cd android

mkdir -p "./Binaries/Client/Android" && rm -rf "./Binaries/Client/Android/*"
cp -r "$SOURCE_FULL_PATH/BuildScripts/android-project/." "./Binaries/Client/Android"

if [ ! -f "$ANDROID_NDK_VERSION-linux-x86_64.zip" ]; then
	wget "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
	unzip "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
	wget "https://dl.google.com/android/repository/$ANDROID_SDK_VERSION-linux.zip"
	mkdir -p sdk
	unzip "$ANDROID_SDK_VERSION-linux.zip" -d "./sdk"
fi
export ANDROID_NDK="$PWD/$ANDROID_NDK_VERSION"
export ANDROID_HOME="$PWD/sdk"

cd sdk/tools
( while sleep 3; do echo "y"; done ) | ./android update sdk --no-ui
cd ../
cd ../

export ANDROID_ABI=armeabi-v7a
mkdir -p $ANDROID_ABI
cd $ANDROID_ABI

pwd

cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/android.cache.cmake" "$SOURCE_FULL_PATH/Source"
make -j4
cd ../

export ANDROID_ABI=x86
mkdir -p $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/android.cache.cmake" "$SOURCE_FULL_PATH/Source"
make -j4
cd ../
