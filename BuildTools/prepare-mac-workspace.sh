#!/bin/bash -e

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/setup-env.sh"
source "$CUR_DIR/internal-tools.sh"

while true; do
    ready=1

    if [[ -z $(xcode-select -p) ]]; then
        echo "Please install Xcode from AppStore"
        ready=0
    fi

    if [[ -x $(command -v cmake) ]]; then
        CMAKE=cmake
    else
        CMAKE=/Applications/CMake.app/Contents/bin/cmake
    fi
    if [[ -z $CMAKE ]]; then
        echo "Please install CMake from https://cmake.org"
        ready=0
    fi

    if [[ $ready = "1" ]]; then
        echo "Workspace is ready!"
        exit 0
    elif [[ ! -z `check_arg check` ]]; then
        echo "Workspace is not ready"
        exit 1
    fi
    
    read -p "..."
done
