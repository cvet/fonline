#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

TARGET="CompileAllScripts"
if [ "$1" = "gen-api" ]; then
    TARGET="GenerateScriptApi"
elif [ "$1" = "angelscript" ]; then
    TARGET="CompileAngelScripts"
elif [ "$1" = "mono" ]; then
    TARGET="CompileMonoScripts"
fi

cd maintenance-env
cmake.exe --build . --config Release --target $TARGET
