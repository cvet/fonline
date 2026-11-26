#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/find-python.sh"

eval "$($PYTHON_BIN "$CUR_DIR/buildtools.py" env --shell bash)"
"$PYTHON_BIN" "$CUR_DIR/buildtools.py" env --summary-only
