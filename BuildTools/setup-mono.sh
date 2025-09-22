#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/setup-env.sh"

if [[ $# -ne 3 ]]; then
    echo "Usage: setup-mono.sh <os> <arch> <config>"
    exit 1
fi

TRIPLET="$1.$2.$3"

# Clone dotnet repo
if [[ ! -f "CLONED" ]]; then
    if [[ -d "runtime" ]]; then
        echo "Remove previous repository"
        rm -rf "runtime"
    fi

    if [[ -z "$FO_DOTNET_RUNTIME_ROOT" ]]; then
        echo "Clone runtime"
        git clone https://github.com/dotnet/runtime.git --depth 1 --branch $FO_DOTNET_RUNTIME
    else
        echo "Copy runtime"
        cp -rf "$FO_DOTNET_RUNTIME_ROOT" "runtime"
    fi
fi

touch "CLONED"

# Build some configuration
if [[ ! -f "BUILT_$TRIPLET" ]]; then
    echo "Build runtime"

    cd "runtime"
    ./build.sh -os $1 -arch $2 -c $3 -subset mono.runtime
    cd ..
fi

touch "BUILT_$TRIPLET"

# Copy built files
copy_runtime() {
    if [[ -d "$1" ]]; then
        echo "Copy from $1 to $2"
        mkdir -p "$2"
        cp -rf "$1/"* "$2"
    else
        echo "Files not found: $1"
        exit 1
    fi
}

if [[ ! -f "READY_$TRIPLET" ]]; then
    echo "Copy runtime"
    copy_runtime "runtime/artifacts/obj/mono/$TRIPLET/out" "output/mono/$TRIPLET"
fi

touch "READY_$TRIPLET"

echo "Runtime $TRIPLET is ready!"
