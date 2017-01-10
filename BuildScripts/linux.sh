#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E "$FO_SOURCE/BuildScripts/linux.sh"

export FO_SDK="$FO_SOURCE/../FOnline"

if [ "$EUID" -ne 0 ]; then
	echo "Run as root"
	exit
fi

apt-get -y update
apt-get -y install build-essential
apt-get -y install cmake
apt-get -y install libx11-dev
apt-get -y install freeglut3-dev
apt-get -y install libssl-dev
apt-get -y install libevent-dev
# x86
apt-get -y install gcc-multilib
apt-get -y install g++-multilib
apt-get -y install libx11-dev:i386
apt-get -y install freeglut3-dev:i386
apt-get -y install libssl-dev:i386
apt-get -y install libevent-dev:i386

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf linux
fi
mkdir linux
cd linux

#mkdir x86
#cd x86
#cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/linux32.cache.cmake" "$FO_SOURCE/Source" && make
#cd ../

mkdir x64
cd x64
cmake -G "Unix Makefiles" -C "$FO_SOURCE/BuildScripts/linux64.cache.cmake" "$FO_SOURCE/Source" && make
cd ../

cp -r ./Client/. "$FO_SDK/Binaries/Client/Linux"
cp -r ./Server/. "$FO_SDK/Binaries/Server"
cp -r ./Mapper/. "$FO_SDK/Binaries/Mapper"
cp -r ./ASCompiler/. "$FO_SDK/Binaries/ASCompiler"
