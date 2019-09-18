#!/bin/bash -e

export FO_CODE_COVERAGE=1
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source "$CUR_DIR/linux.sh"

echo "Run code coverage"
cd $FO_BUILD_DEST
$FO_BUILD_DEST/Binaries/Tests/FOnlineCodeCoverage

if [[ -z "$CODECOV_TOKEN" ]]; then
	echo "Upload reports to codecov.io"
	bash <(curl -s https://codecov.io/bash)
fi
