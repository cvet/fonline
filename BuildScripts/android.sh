#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && export ANDROID_NDK=<ndk> && $FO_SOURCE/BuildScripts/android.sh

rm -rf android
mkdir android
cd android

mkdir android-project
cp -r "$FO_SOURCE/BuildScripts/android-project" "./"

export ANDROID_ABI=armeabi-v7a
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -C $FO_SOURCE/BuildScripts/android.cache.cmake $FO_SOURCE/Source && make
cd ../

export ANDROID_ABI=arm64-v8a
mkdir $ANDROID_ABI
cd $ANDROID_ABI
cmake -C $FO_SOURCE/BuildScripts/android.cache.cmake $FO_SOURCE/Source && make
cd ../
