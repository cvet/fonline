#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E "$FO_SOURCE/BuildScripts/web.sh"

export FO_SDK="$FO_SOURCE/../FOnline"

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

apt-get -y update
apt-get -y install build-essential
apt-get -y install cmake
apt-get -y install python2.7
apt-get -y install nodejs
apt-get -y install default-jre
apt-get -y install git-core

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

cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/web.cache.cmake" "$FO_SOURCE/Source" && make

cp -r Web "$FO_SDK/Binaries/Client"
