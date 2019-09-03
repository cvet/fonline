#!/bin/bash -ex

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	sudo apt-get -y update || true
	sudo apt-get -y install build-essential
	sudo apt-get -y install cmake
	sudo apt-get -y install wput
	sudo apt-get -y install libx11-dev
	sudo apt-get -y install freeglut3-dev
	sudo apt-get -y install libssl-dev
	sudo apt-get -y install libevent-dev
	sudo apt-get -y install libxi-dev
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p linux
cd linux
rm -rf Client/*
rm -rf Server/*
rm -rf Mapper/*
rm -rf ASCompiler/*

mkdir -p x64
cd x64
cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/linux64.cache.cmake" "$ROOT_FULL_PATH"
make -j6
cd ../

# x86 (Temporarily disabled)
#if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
#	sudo apt-get -y install gcc-multilib
#	sudo apt-get -y install g++-multilib
#	sudo apt-get -y install libx11-dev:i386
#	sudo apt-get -y install freeglut3-dev:i386
#	sudo apt-get -y install libssl-dev:i386
#	sudo apt-get -y install libevent-dev:i386
#	sudo apt-get -y install libxi-dev:i386
#fi

#mkdir -p x86
#cd x86
#cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/linux32.cache.cmake" "$ROOT_FULL_PATH"
#make -j6
#cd ../
