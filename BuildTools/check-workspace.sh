#!/bin/bash -e

echo "Check workspace status"

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ -f "$FO_WORKSPACE/workspace-version.txt" ]; then
    VER=`cat $FO_WORKSPACE/workspace-version.txt`
    if [[ "$VER" = "$FO_WORKSPACE_VERSION" ]]; then
        echo "Workspace is actual"
        exit
    fi

    echo "Workspace is outdated"
    exit 11
fi

echo "Workspace is not created"
exit 10
