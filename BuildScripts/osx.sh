#!/bin/bash

# Usage:
# export FO_SOURCE=<source> && sudo -E $FO_SOURCE/BuildScripts/mac.sh

if [ "$FO_CLEAR" = "TRUE" ]; then
	rm -rf mac
fi
mkdir mac
cd mac

/Applications/CMake.app/Contents/bin/cmake -G Xcode $FO_SOURCE/Source
/Applications/CMake.app/Contents/bin/cmake --build . --config RelWithDebInfo --target FOnline
