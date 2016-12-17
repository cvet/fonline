#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && $FO_SOURCE/BuildScripts/web.sh

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf web
fi
mkdir web
cd web

mkdir emscripten
cp -r "$FO_SOURCE/BuildScripts/emsdk" "./"
cd emscripten
./emsdk update
./emsdk install latest
./emsdk activate latest
cd ../
emcc -v

cmake -C $FO_SOURCE/BuildScripts/web.cache.cmake $FO_SOURCE/Source && make
cd ../
