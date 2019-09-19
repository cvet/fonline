#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "arm32" ] && [ ! "$1" = "arm64" ]; then
	echo "Invalid build argument, allowed only arm32 or arm64"
	exit 1
fi

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build
[ "$FO_INSTALL_PACKAGES" ] || export FO_INSTALL_PACKAGES=1

echo "Setup environment"
export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)
export ANDROID_NDK_VERSION="android-ndk-r18b"
export ANDROID_SDK_VERSION="tools_r25.2.3"
export ANDROID_NATIVE_API_LEVEL_NUMBER=21
export ANDROID_HOME="$PWD/Android/sdk"

if [[ "$FO_INSTALL_PACKAGES" = "1" ]]; then
	echo "Install packages"
	echo "Sudo required"
	sudo apt-get -qq -y update || true
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
fi

mkdir -p $FO_BUILD_DEST && cd $FO_BUILD_DEST

echo "Copy placeholder"
mkdir -p "./Binaries/Client/Android" && rm -rf "./Binaries/Client/Android/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/android-project/." "./Binaries/Client/Android"

mkdir -p Android && cd Android

if [ ! -f "$ANDROID_NDK_VERSION-linux-x86_64.zip" ]; then
	echo "Download Android NDK"
	wget -q "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip"
	echo "Unzip Android NDK"
	unzip -qq "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"

	echo "Download Android SDK"
	wget -q "https://dl.google.com/android/repository/$ANDROID_SDK_VERSION-linux.zip"
	echo "Unzip Android SDK"
	mkdir -p sdk
	unzip -qq "$ANDROID_SDK_VERSION-linux.zip" -d "./sdk"
	echo "Update Android SDK"
	cd sdk/tools
	( while sleep 3; do echo "y"; done ) | ./android update sdk --no-ui
	cd ../../

	echo "Generate toolchains"
	cd $ANDROID_NDK_VERSION/build/tools
	python make_standalone_toolchain.py --arch arm --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm-toolchain
	python make_standalone_toolchain.py --arch arm64 --api $ANDROID_NATIVE_API_LEVEL_NUMBER --install-dir ../../../arm64-toolchain
	cd ../../../
fi

if [ "$1" = "" ] || [ "$1" = "arm32" ]; then
	echo "Build Arm32 binaries"
	export ANDROID_STANDALONE_TOOLCHAIN=$PWD/arm-toolchain
	export ANDROID_ABI=armeabi-v7a
	mkdir -p $ANDROID_ABI && cd $ANDROID_ABI
	cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
	make -j$(nproc)
	cd ../
fi

if [ "$1" = "" ] || [ "$1" = "arm64" ]; then
	echo "Build Arm64 binaries"
	export ANDROID_STANDALONE_TOOLCHAIN=$PWD/arm64-toolchain
	export ANDROID_ABI=arm64-v8a
	mkdir -p $ANDROID_ABI && cd $ANDROID_ABI
	cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/android.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "$ROOT_FULL_PATH"
	make -j$(nproc)
	cd ../
fi
