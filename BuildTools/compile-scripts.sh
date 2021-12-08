#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

TARGET="CompileAllScripts"
if [ "$1" = "gen-code" ]; then
    TARGET="GenerateScriptingCode"
elif [ "$1" = "native" ]; then
    TARGET="CompileNativeScripts"
elif [ "$1" = "angelscript" ]; then
    TARGET="CompileAngelScripts"
elif [ "$1" = "mono" ]; then
    TARGET="CompileMonoScripts"
fi

if grep -q icrosoft /proc/version; then
	cd build-win64-toolset
	cmake.exe --build . --config Release --target $TARGET
else
	cd build-linux-toolset
	cmake --build . --config Release --target $TARGET
fi
