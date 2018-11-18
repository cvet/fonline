#!/bin/bash -ex

[ "$FO_ROOT" ] || { echo "FO_ROOT variable is not set"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST variable is not set"; exit 1; }

export ROOT_FULL_PATH=$(cd $FO_ROOT; pwd)

if [ -n "$FO_INSTALL_PACKAGES" ]; then
	#sudo apt-get -y update || true
	#sudo apt-get -y install build-essential
	#sudo apt-get -y install cmake
	#sudo apt-get -y install wput
	#sudo apt-get -y install git
fi

mkdir -p $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir -p raspberrypi
cd raspberrypi
mkdir -p RaspberryPi
rm -rf RaspberryPi/*

if [ ! -d "./tools" ]; then
	git clone https://github.com/raspberrypi/tools
fi
export PI_TOOLS_HOME=$PWD/tools

cmake -G "Unix Makefiles" -C "$ROOT_FULL_PATH/BuildScripts/raspberrypi.cache.cmake" "$ROOT_FULL_PATH/Source"
make -j4

if [ -n "$FO_FTP_DEST" ]; then
	wput RaspberryPi ftp://$FO_FTP_USER@$FO_FTP_DEST/Client/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r RaspberryPi "$FO_COPY_DEST/Client"
fi
