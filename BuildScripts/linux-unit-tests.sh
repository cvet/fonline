#!/bin/bash -e

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/linux.sh unit-tests

echo "Run unit tests"
cd $FO_WORKSPACE
$FO_OUTPUT_PATH/Tests/FOnlineUnitTests
