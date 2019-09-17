#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

export ANDROID_NDK_VERSION="android-ndk-r18b"
export ANDROID_SDK_VERSION="tools_r25.2.3"
export ANDROID_NATIVE_API_LEVEL_NUMBER=21

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	echo apt-get update
	(sudo apt-get -y update > /dev/null) || true
	echo install build-essential
	sudo apt-get -y install build-essential > /dev/null
	echo install cmake
	sudo apt-get -y install cmake > /dev/null
	echo install unzip
	sudo apt-get -y install unzip > /dev/null
	echo install wput
	sudo apt-get -y install wput > /dev/null
	echo install ant
	sudo apt-get -y install ant > /dev/null
	echo install openjdk-8-jdk
	sudo apt-get -y install openjdk-8-jdk > /dev/null
	echo install python
	sudo apt-get -y install python > /dev/null
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

mkdir -p "./Binaries/Client/Android" && rm -rf "./Binaries/Client/Android/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/android-project/." "./Binaries/Client/Android"

mkdir -p Android
cd Android

if [ ! -f "$ANDROID_NDK_VERSION-linux-x86_64.zip" ]; then
	wget "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
	unzip "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
	wget "https://dl.google.com/android/repository/$ANDROID_SDK_VERSION-linux.zip"
	mkdir -p sdk
	unzip "$ANDROID_SDK_VERSION-linux.zip" -d "./sdk"

	cd sdk/tools
	( while sleep 3; do echo "y"; done ) | ./android update sdk --no-ui
	cd ../../

	cd $ANDROID_NDK_VERSION/build/tools
	python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm-toolchain
	python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm64-toolchain
	cd ../../../
fi

export ANDROID_HOME="$PWD/sdk"

export ANDROID_STANDALONE_TOOLCHAIN=$PWD/arm-toolchain
export ANDROID_ABI=armeabi-v7a
mkdir -p $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../

export ANDROID_STANDALONE_TOOLCHAIN=$PWD/arm64-toolchain
export ANDROID_ABI=arm64-v8a
mkdir -p $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../
