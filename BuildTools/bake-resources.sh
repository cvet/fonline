#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if grep -q icrosoft /proc/version; then
	cd build-win64-toolset
	cmake.exe --build . --config Release --target BakeResources
else
	cd build-linux-toolset
	cmake --build . --config Release --target BakeResources
fi
