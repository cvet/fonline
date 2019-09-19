#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "unit-tests" ] && [ ! "$1" = "code-coverage" ]; then
	echo "Invalid build argument, allowed only unit-tests or code-coverage"
	exit 1
fi

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build
[ "$FO_INSTALL_PACKAGES" ] || export FO_INSTALL_PACKAGES=1

echo "Setup environment"
export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

if [[ "$FO_INSTALL_PACKAGES" = "1" ]]; then
	echo "Install packages"
	echo "Sudo required"
	sudo apt-get -qq -y update || true
	echo "Install clang"
	sudo apt-get -qq -y install clang
	echo "Install build-essential"
	sudo apt-get -qq -y install build-essential
	echo "Install cmake"
	sudo apt-get -qq -y install cmake
	echo "Install wput"
	sudo apt-get -qq -y install wput
	echo "Install libx11-dev"
	sudo apt-get -qq -y install libx11-dev
	echo "Install freeglut3-dev"
	sudo apt-get -qq -y install freeglut3-dev
	echo "Install libssl-dev"
	sudo apt-get -qq -y install libssl-dev
	echo "Install libevent-dev"
	sudo apt-get -qq -y install libevent-dev
	echo "Install libxi-dev"
	sudo apt-get -qq -y install libxi-dev
	echo "Install curl"
	sudo apt-get -qq -y install curl
	echo "Install binutils-dev"
	sudo apt-get -qq -y install binutils-dev
fi

mkdir -p $FO_BUILD_DEST && cd $FO_BUILD_DEST
mkdir -p Linux && cd Linux

if [ "$1" = "unit-tests" ]; then
	echo "Build unit tests binaries"
	mkdir -p unit-tests && cd unit-tests
	cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_UNIT_TESTS=1 "$ROOT_FULL_PATH"
elif [ "$1" = "code-coverage" ]; then
	echo "Build code coverage binaries"
	mkdir -p code-coverage && cd code-coverage
	cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_CODE_COVERAGE=1 "$ROOT_FULL_PATH"
else
	echo "Build default binaries"
	mkdir -p default && cd default
	cmake -G "Unix Makefiles" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_UNIT_TESTS=0 -DFONLINE_CODE_COVERAGE=0 "$ROOT_FULL_PATH"
fi

make -j$(nproc)
