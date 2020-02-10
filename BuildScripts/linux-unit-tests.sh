#!/bin/bash -e

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
$CUR_DIR/linux.sh unit-tests

echo "Run unit tests"
cd $FO_BUILD_DEST
./Binaries/Tests/FOnlineUnitTests
