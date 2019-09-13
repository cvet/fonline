#!/bin/bash -e

[ "$FO_ROOT" ] || { [[ -e CMakeLists.txt ]] && { export FO_ROOT=. || true ;} ;} || export FO_ROOT=../
[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

export EMSCRIPTEN_VERSION="sdk-1.38.31-64bit"

if [[ -z "$FO_INSTALL_PACKAGES" ]]; then
	sudo apt-get -y update || true
	sudo apt-get -y install build-essential
	sudo apt-get -y install cmake
	sudo apt-get -y install wput
	sudo apt-get -y install python
	sudo apt-get -y install nodejs
	sudo apt-get -y install default-jre
	sudo apt-get -y install git-core
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST

mkdir -p "./Binaries/Client/Web" && rm -rf "./Binaries/Client/Web/*"
cp -r "$ROOT_FULL_PATH/BuildScripts/web/." "./Binaries/Client/Web"

mkdir -p Web
cd Web

mkdir -p emsdk
cp -r "$ROOT_FULL_PATH/BuildScripts/emsdk" "./"
cd emsdk
chmod +x ./emsdk
./emsdk update
./emsdk install $EMSCRIPTEN_VERSION
./emsdk activate $EMSCRIPTEN_VERSION
source ./emsdk_env.sh
cd ../
emcc -v

mkdir -p release
cd release
cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_WEB_DEBUG=OFF "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../

mkdir -p debug
cd debug
cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/web.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" -DFONLINE_WEB_DEBUG=ON "$ROOT_FULL_PATH"
make -j$(nproc)
cd ../
