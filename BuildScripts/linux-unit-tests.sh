#!/bin/bash -e

export FO_UNIT_TESTS=1
source ./linux.sh

echo "Run unit tests"
./Build/Binaries/Tests/FOnlineUnitTests
