#!/bin/bash -e

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $CUR_DIR/setup-env.sh
$CUR_DIR/linux.sh code-coverage

echo "Run code coverage"
cd $FO_WORKSPACE
./Binaries/Tests/FOnlineCodeCoverage

if [[ ! -z "$CODECOV_TOKEN" ]]; then
    echo "Upload reports to codecov.io"
    bash <(curl -s https://codecov.io/bash)
fi
