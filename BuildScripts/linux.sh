#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	echo apt-get update
	sudo apt-get -qq -y update || true
	echo install clang
	sudo apt-get -qq -y install clang
	echo install build-essential
	sudo apt-get -qq -y install build-essential
	echo install cmake
	sudo apt-get -qq -y install cmake
	echo install wput
	sudo apt-get -qq -y install wput
	echo install libx11-dev
	sudo apt-get -qq -y install libx11-dev
	echo install freeglut3-dev
	sudo apt-get -qq -y install freeglut3-dev
	echo install libssl-dev
	sudo apt-get -qq -y install libssl-dev
	echo install libevent-dev
	sudo apt-get -qq -y install libevent-dev
	echo install libxi-dev
	sudo apt-get -qq -y install libxi-dev
	echo install curl
	sudo apt-get -qq -y install curl
	echo install binutils-dev
	sudo apt-get -qq -y install binutils-dev
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

# export CC=/usr/bin/clang
# export CXX=/usr/bin/clang++

mkdir -p Linux
cd Linux
cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../" "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../
