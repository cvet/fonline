#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && export ANDROID_NDK=<ndk> && android.sh

cmake -C $FO_SOURCE/Android/android.cache.cmake $FO_SOURCE
make
