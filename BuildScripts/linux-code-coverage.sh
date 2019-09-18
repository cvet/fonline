#!/bin/bash -e

[ "$FO_BUILD_DEST" ] || export FO_BUILD_DEST=Build

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
$CUR_DIR/linux.sh code-coverage

echo "Run code coverage"
cd $FO_BUILD_DEST
./Binaries/Tests/FOnlineCodeCoverage

if [[ -z "$CODECOV_TOKEN" ]]; then
	echo "Upload reports to codecov.io"
	bash <(curl -s https://codecov.io/bash)
fi
