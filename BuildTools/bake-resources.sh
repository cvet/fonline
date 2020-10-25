#!/bin/bash -e

CUR_DIR="$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)"
source $CUR_DIR/setup-env.sh

cd maintenance-env
cmake.exe --build . --config Release --target BakeResources
