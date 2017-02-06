#!/bin/bash

export SOURCE_FULL_PATH=$(cd $FO_SOURCE; pwd)

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

apt-get -y update
apt-get -y install build-essential
apt-get -y install cmake
apt-get -y install wput
apt-get -y install python2.7
apt-get -y install nodejs
apt-get -y install default-jre
apt-get -y install git-core

mkdir $FO_BUILD_DEST
cd $FO_BUILD_DEST
mkdir web
cd web

mkdir emsdk
cp -r "$SOURCE_FULL_PATH/BuildScripts/emsdk" "./"
cd emsdk
./emsdk update
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ../
emcc -v

cmake -G "Unix Makefiles" -C "$SOURCE_FULL_PATH/BuildScripts/web.cache.cmake" "$SOURCE_FULL_PATH/Source" && make

if [ -n "$FO_FTP_DEST" ]; then
	wput Web ftp://$FO_FTP_USER@$FO_FTP_DEST/Client/
fi

if [ -n "$FO_COPY_DEST" ]; then
	cp -r Web "$FO_COPY_DEST/Client"
fi
