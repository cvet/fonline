#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E "$FO_SOURCE/BuildScripts/web.sh"

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

sudo apt-get -y update
sudo apt-get -y install build-essential
sudo apt-get -y install cmake
sudo apt-get -y install python2.7
sudo apt-get -y install nodejs
sudo apt-get -y install default-jre
sudo apt-get -y install git-core

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf web
fi
mkdir web
cd web

mkdir emsdk
cp -r "$FO_SOURCE/BuildScripts/emsdk" "./"
cd emsdk
./emsdk update
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ../
emcc -v

cmake -C $FO_SOURCE/BuildScripts/web.cache.cmake $FO_SOURCE/Source && make
