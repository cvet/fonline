#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && export ANDROID_NDK=<ndk> && $FO_SOURCE/BuildScripts/android.sh

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf android
fi
mkdir android
cd android

mkdir android-project
cp -r "$FO_SOURCE/BuildScripts/android-project" "./"

export ANDROID_ABI=armeabi-v7a
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -C $FO_SOURCE/BuildScripts/android.cache.cmake $FO_SOURCE/Source && make
cd ../

export ANDROID_ABI=x86
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -C $FO_SOURCE/BuildScripts/android.cache.cmake $FO_SOURCE/Source && make
cd ../
