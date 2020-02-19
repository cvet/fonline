#!/bin/bash -e

if [ "$1" = "" ]; then
    echo "Provide at least one argument"
    exit 1
fi

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ "$1" = "win" ]; then

elif [ "$1" = "uwp" ]; then

elif [ "$1" = "linux" ]; then

elif [ "$1" = "web" ]; then
    echo "Copy Web placeholder"
    mkdir -p "$OUTPUT_PATH/Client/Web" && rm -rf "$OUTPUT_PATH/Client/Web/*"
    cp -r "$FO_ROOT/BuildTools/web/." "$OUTPUT_PATH/Client/Web"

elif [ "$1" = "android" ]; then
    echo "Copy Android placeholder"
    mkdir -p "$OUTPUT_PATH/Client/Android" && rm -rf "$OUTPUT_PATH/Client/Android/*"
    cp -r "$FO_ROOT/BuildTools/android-project/." "$OUTPUT_PATH/Client/Android"

elif [ "$1" = "mac" ]; then

elif [ "$1" = "ios" ]; then

fi
