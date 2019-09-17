#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	echo apt-get update
	(sudo apt-get -y update > /dev/null) || true
	echo install clang
	sudo apt-get -y install clang > /dev/null
	echo install build-essential
	sudo apt-get -y install build-essential > /dev/null
	echo install cmake
	sudo apt-get -y install cmake > /dev/null
	echo install wput
	sudo apt-get -y install wput > /dev/null
	echo install libx11-dev
	sudo apt-get -y install libx11-dev > /dev/null
	echo install freeglut3-dev
	sudo apt-get -y install freeglut3-dev > /dev/null
	echo install libssl-dev
	sudo apt-get -y install libssl-dev > /dev/null
	echo install libevent-dev
	sudo apt-get -y install libevent-dev > /dev/null
	echo install libxi-dev
	sudo apt-get -y install libxi-dev > /dev/null
	echo install curl
	sudo apt-get -y install curl > /dev/null
	echo install binutils-dev
	sudo apt-get -y install binutils-dev > /dev/null
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

mkdir -p Linux
cd Linux
cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../" "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../
