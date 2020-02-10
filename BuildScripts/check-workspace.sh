#!/bin/bash -e

echo "Check workspace status"

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh

if [ -f "$FO_BUILD_DEST/EnvVersion.txt" ]; then
    VER=`cat $FO_BUILD_DEST/EnvVersion.txt`
    if [[ "$VER" = "$FO_ENV_VERSION" ]]; then
        echo "Workspace is actual"
        exit
    fi

    echo "Workspace is outdated"
    exit 12
fi

if [ -d "$FO_BUILD_DEST" ]; then
    echo "Workspace is not fully created"
    exit 11
else
    echo "Workspace is not created"
    exit 10
fi
