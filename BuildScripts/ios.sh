#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E $FO_SOURCE/BuildScripts/ios.sh

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf iOS
fi
mkdir ios
cd ios

/Applications/CMake.app/Contents/bin/cmake -G Xcode -C "$FO_SOURCE/BuildScripts/ios.cache.cmake" $FO_SOURCE/Source
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline
