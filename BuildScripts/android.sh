#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E "$FO_SOURCE/BuildScripts/android.sh"

export ANDROID_NDK_VERSION="android-ndk-r12b"

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

sudo apt-get -y update
sudo apt-get -y install build-essential
sudo apt-get -y install cmake

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf android
fi
mkdir android
cd android

if [ ! -f "$ANDROID_NDK_VERSION-linux-x86_64.zip" ]; then
	wget "https://dl.google.com/android/repository/$ANDROID_NDK_VERSION-linux-x86_64.zip" 
	sudo apt-get -y install unzip
	unzip "$ANDROID_NDK_VERSION-linux-x86_64.zip" -d "./"
fi
export ANDROID_NDK="$PWD/$ANDROID_NDK_VERSION"

mkdir android-project
cp -r "$FO_SOURCE/BuildScripts/android-project" "./"

export ANDROID_ABI=armeabi-v7a
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/android.cache.cmake" "$FO_SOURCE/Source" && make
cd ../

export ANDROID_ABI=x86
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/android.cache.cmake" "$FO_SOURCE/Source" && make
cd ../
