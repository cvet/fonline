#!/bin/bash -ex

[ "$FO_SOURCE" ] || { echo "FO_SOURCE is empty"; exit 1; }
[ "$FO_BUILD_DEST" ] || { echo "FO_BUILD_DEST is empty"; exit 1; }

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

#sudo apt-get -y update || true
#sudo apt-get -y install build-essential
#sudo apt-get -y install cmake
#sudo apt-get -y install wput
#sudo apt-get -y install git

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

cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/raspberrypi.cache.cmake" "$SOURCE_FULL_PATH/Source"
make -j4

if [ -n "$FO_FTP_DEST" ]; then
	wput RaspberryPi ftp://$FO_FTP_USER@$FO_FTP_DEST/Client/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r RaspberryPi "$FO_COPY_DEST/Client"
fi
