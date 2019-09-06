#!/bin/bash -e

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
	sudo apt-get -y install curl
	sudo apt-get -y install binutils-dev
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

mkdir -p Linux
cd Linux
cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../" "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../
