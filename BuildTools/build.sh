#!/bin/bash -e

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(command -v python3 || command -v python || true)"

if [[ -z "$PYTHON_BIN" ]]; then
	echo "Python not found" >&2
	exit 1
fi

exec "$PYTHON_BIN" "$CUR_DIR/buildtools.py" build "$@"
