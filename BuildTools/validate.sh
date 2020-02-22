#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ "$1" = "linux" ]; then
    TARGET=linux
    TARGET_ARG=full
elif [ "$1" = "android-arm" ]; then
    TARGET=android
elif [ "$1" = "android-arm64" ]; then
    TARGET=android-arm64
elif [ "$1" = "android-x86" ]; then
    TARGET=android-x86
elif [ "$1" = "web" ]; then
    TARGET=web
elif [ "$1" = "mac" ]; then
    TARGET=mac
elif [ "$1" = "ios" ]; then
    TARGET=ios
elif [ "$1" = "unit-tests" ]; then
    TARGET=linux
    TARGET_ARG=unit-tests
elif [ "$1" = "code-coverage" ]; then
    TARGET=linux
    TARGET_ARG=code-coverage
else
    echo "Invalid argument"
fi

if [ "$1" = "mac" ] && [ "$1" != "ios" ]; then
    $CUR_DIR/prepare-workspace.sh $TARGET
fi

$CUR_DIR/build.sh $TARGET $TARGET_ARG

if [ "$1" = "unit-tests" ]; then
    echo "Run unit tests"
    $OUTPUT_PATH/Tests/FOnlineUnitTests

elif [ "$1" = "code-coverage" ]; then
    echo "Run code coverage"
    $OUTPUT_PATH/Tests/FOnlineCodeCoverage

    if [[ ! -z "$CODECOV_TOKEN" ]]; then
        echo "Upload reports to codecov.io"
        bash <(curl -s https://codecov.io/bash)
    fi
fi
