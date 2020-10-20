#!/bin/bash -e

echo "Prepare Visual Studio toolset"

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh
source $CUR_DIR/tools.sh

function generate_compilation_env()
{
    echo "Generate VSCode compilation environment"
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    FO_CMAKE_CONTRIBUTION_WIN=`wsl_path_to_windows "$FO_CMAKE_CONTRIBUTION"`
    rm -rf vscode-compilation-env
    mkdir vscode-compilation-env
    cd vscode-compilation-env
    cmake.exe -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Debug -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_MAPPER=1 -DFONLINE_BUILD_BAKER=1 -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION_WIN" "$FO_ROOT_WIN"
    cmake.exe --build . --config Debug --parallel
}

function generate_vscode_js_toolset()
{
    echo "Generate VSCode js toolset"
    rm -rf vscode-js-toolset
    mkdir vscode-js-toolset
    cd vscode-js-toolset
    source $FO_WORKSPACE/emsdk/emsdk_env.sh
    cmake -G "Unix Makefiles" -C "$FO_ROOT/BuildTools/web.cache.cmake" -DCMAKE_BUILD_TYPE=Release -DFONLINE_BUILD_MAPPER=1 -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION" "$FO_ROOT"
    cmake --build . --config Release --parallel
}

function generate_vscode_native_toolset()
{
    echo "Generate VSCode native toolset"
    FO_ROOT_WIN=`wsl_path_to_windows "$FO_ROOT"`
    FO_CMAKE_CONTRIBUTION_WIN=`wsl_path_to_windows "$FO_CMAKE_CONTRIBUTION"`
    rm -rf vscode-native-toolset
    mkdir vscode-native-toolset
    cd vscode-native-toolset
    cmake.exe -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_BAKER=1 -DFONLINE_CMAKE_CONTRIBUTION="$FO_CMAKE_CONTRIBUTION_WIN" "$FO_ROOT_WIN"
    cmake.exe --build . --config Release --parallel
}

verify_workspace_part compilation-env 2 generate_compilation_env
verify_workspace_part vscode-js-toolset 2 generate_vscode_js_toolset
verify_workspace_part vscode-native-toolset 2 generate_vscode_native_toolset
wait_jobs

echo "Visual Studio Code toolset is ready!"
