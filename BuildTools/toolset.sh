#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/setup-env.sh"

mkdir -p "$FO_WORKSPACE"
pushd "$FO_WORKSPACE"

if [[ -d "emsdk" ]]; then
    echo ""
    echo "Found EMSDK, apply it"
    echo ""
    source emsdk/emsdk_env.sh
    echo ""
fi

cd "build-linux-toolset"
cmake --build . --config Release --target $1 --parallel

popd
