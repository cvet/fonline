#!/bin/bash

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

export ANDROID_NDK_VERSION="android-ndk-r12b"
export ANDROID_SDK_VERSION="tools_r25.2.3"

sudo -S true
sudo apt-get -y update
sudo apt-get -y install build-essential
sudo apt-get -y install cmake
sudo apt-get -y install wput
sudo apt-get -y install ant
sudo apt-get -y install openjdk-8-jdk

mkdir $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir android
cd android

if [ ! -f "$ANDROID_NDK_VERSION-linux-x86_64.zip" ]; then
	sudo apt-get -y install unzip
	wget "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip" 
	unzip "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
	wget "https://dl.google.com/android/repository/$ANDROID_SDK_VERSION-linux.zip" 
	mkdir sdk
	unzip "$ANDROID_SDK_VERSION-linux.zip" -d "./sdk"
fi
export ANDROID_NDK="$PWD/$ANDROID_NDK_VERSION"
export ANDROID_HOME="$PWD/sdk"

cd sdk/tools
echo y | ./android update sdk --no-ui
cd ../
cd ../

rm -rf Android/*
mkdir Android
cp -r "$SOURCE_FULL_PATH/BuildScripts/android-project/." "./Android/"

export ANDROID_ABI=armeabi-v7a
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/android.cache.cmake" "$SOURCE_FULL_PATH/Source"
make
cd ../

export ANDROID_ABI=x86
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/android.cache.cmake" "$SOURCE_FULL_PATH/Source"
make
cd ../

if [ -n "$FO_FTP_DEST" ]; then
	wput Android ftp://$FO_FTP_USER@$FO_FTP_DEST/Client/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r Android "$FO_COPY_DEST/Client"
fi
