#!/bin/bash -e

[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
$CUR_DIR/linux.sh unit-tests

echo "Run unit tests"
cd $FO_BUILD_DEST
./Binaries/Tests/FOnlineUnitTests
