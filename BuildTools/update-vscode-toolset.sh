#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

function build_vscode_js_toolset()
{
    cd vscode-js-toolset
    source $FO_WORKSPACE/emsdk/emsdk_env.sh
    cmake --build . --config Release --parallel
}

function build_vscode_native_toolset()
{
    cd vscode-native-toolset
    cmake.exe --build . --config Release --parallel
}

run_job build_vscode_js_toolset
run_job build_vscode_native_toolset
wait_jobs
