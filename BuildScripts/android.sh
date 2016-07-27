#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && export ANDROID_NDK=<ndk> && $FO_SOURCE/BuildScripts/android.sh

rm -rf android && mkdir android
cd android
cmake -C $FO_SOURCE/BuildScripts/android.cache.cmake $FO_SOURCE/Source && make
