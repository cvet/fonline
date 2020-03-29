#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

function build_vscode_js_toolset()
{
    cd vscode-js-toolset
    source $FO_WORKSPACE/emsdk/emsdk_env.sh
    cmake --build . --config Release -- -j$(nproc)
}

function build_vscode_native_toolset()
{
    cd vscode-native-toolset
    cmake.exe --build . --config Release
}

run_job build_vscode_js_toolset
run_job build_vscode_native_toolset
wait_jobs
