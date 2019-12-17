#!/bin/bash -e

if [ ! "$1" = "" ] && [ ! "$1" = "release" ] && [ ! "$1" = "debug" ]; then
	echo "Invalid build argument, allowed only release or debug"
	exit 1
fi

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build
[ "$FO_INSTALL_PACKAGES" ] || export FO_INSTALL_PACKAGES=1

echo "Setup environment"
export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)
export EMSCRIPTEN_VERSION="1.39.4"

if [[ "$FO_INSTALL_PACKAGES" = "1" ]]; then
	echo "Install packages"
	echo "Sudo required"
	sudo apt-get -qq -y update || true
	echo "Install build-essential"
	sudo apt-get -qq -y install build-essential
	echo "Install cmake"
	sudo apt-get -qq -y install cmake
	echo "Install wput"
	sudo apt-get -qq -y install wput
	echo "Install python"
	sudo apt-get -qq -y install python
	echo "Install nodejs"
	sudo apt-get -qq -y install nodejs
	echo "Install default-jre"
	sudo apt-get -qq -y install default-jre
	echo "Install git-core"
	sudo apt-get -qq -y install git-core
fi

mkdir -p $FO_BUILD_DEST && cd $FO_BUILD_DEST

echo "Copy placeholder"
mkdir -p "./Binaries/Client/Web" && rm -rf "./Binaries/Client/Web/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/web/." "./Binaries/Client/Web"

mkdir -p Web && cd Web

echo "Setup Emscripten"
mkdir -p emsdk
cp -r "$ROOT_FULL_PATH/BuildScripts/emsdk" "./"
cd emsdk
./emsdk update
./emsdk list
./emsdk install --build=Release --shallow $EMSCRIPTEN_VERSION
./emsdk activate --build=Release --embedded $EMSCRIPTEN_VERSION
source ./emsdk_env.sh
rm -rf releases/.git
cd ../
emcc -v

if [ "$1" = "" ] || [ "$1" = "release" ]; then
	echo "Build release binaries"
	mkdir -p release && cd release
	cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_WEB_DEBUG=OFF "$ROOT_FULL_PATH"
	make -j$(nproc)
	cd ../
fi

if [ "$1" = "" ] || [ "$1" = "debug" ]; then
	echo "Build debug binaries"
	mkdir -p debug && cd debug
	cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_WEB_DEBUG=ON "$ROOT_FULL_PATH"
	make -j$(nproc)
	cd ../
fi
