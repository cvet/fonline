#!/bin/bash -e

export FO_CODE_COVERAGE=1
source ./linux.sh

echo "Run code coverage"
./Build/Binaries/Tests/FOnlineCodeCoverage

if [[ -z "$CODECOV_TOKEN" ]]; then
	echo "Upload reports to codecov.io"
	bash <(curl -s https://codecov.io/bash)
fi
