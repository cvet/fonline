#!/bin/bash -e

export FO_UNIT_TESTS=1
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source "$CUR_DIR/linux.sh"

echo "Run unit tests"
cd $FO_BUILD_DEST
./Binaries/Tests/FOnlineUnitTests
