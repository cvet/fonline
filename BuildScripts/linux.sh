#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E "$FO_SOURCE/BuildScripts/linux.sh"

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

sudo apt-get -y update
sudo apt-get -y install build-essential
sudo apt-get -y install cmake
sudo apt-get -y install libx11-dev
sudo apt-get -y install freeglut3-dev
sudo apt-get -y install libssl-dev
sudo apt-get -y install libevent-dev
# x86
sudo apt-get -y install gcc-multilib
sudo apt-get -y install g++-multilib
sudo apt-get -y install libx11-dev:i386
sudo apt-get -y install freeglut3-dev:i386
sudo apt-get -y install libssl-dev:i386
sudo apt-get -y install libevent-dev:i386

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf linux
fi
mkdir linux
cd linux

mkdir x86
cd x86
cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/linux32.cache.cmake" "$FO_SOURCE/Source" && make
cd ../

mkdir x64
cd x64
cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/linux64.cache.cmake" "$FO_SOURCE/Source" && make
cd ../
