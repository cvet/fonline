#!/bin/bash

# Usage:
# Install: freeglut3-dev, libssl-dev, libevent
# export FO_SOURCE=<source> && $FO_SOURCE/BuildScripts/linux.sh

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf linux
fi
mkdir linux
cd linux

mkdir x86
cd x86
#export CFLAGS=-m32
cmake -G "Unix Makefiles" $FO_SOURCE/Source
cd ../

mkdir x64
cd x64
#export CFLAGS=-m64
cmake -G "Unix Makefiles" $FO_SOURCE/Source
cd ../

cmake --build ./x86 --config RelWithDebInfo --target FOnline
cmake --build ./x86 --config RelWithDebInfo --target FOnlineServer
cmake --build ./x86 --config RelWithDebInfo --target Mapper
cmake --build ./x86 --config RelWithDebInfo --target ASCompiler
