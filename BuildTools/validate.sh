#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

if [ "$1" != "mac" ] && [ "$1" != "ios" ]; then
    $CUR_DIR/prepare-workspace.sh $1
fi

$CUR_DIR/build.sh $1 $2

if [ "$2" = "unit-tests" ]; then
    echo "Run unit tests"
    $OUTPUT_PATH/Tests/FOnlineUnitTests

elif [ "$2" = "code-coverage" ]; then
    echo "Run code coverage"
    $OUTPUT_PATH/Tests/FOnlineCodeCoverage

    if [[ ! -z "$CODECOV_TOKEN" ]]; then
        echo "Upload reports to codecov.io"
        bash <(curl -s https://codecov.io/bash)
    fi
fi
