#!/bin/bash -e

if [[ -z $1 ]]; then
    echo "Provide at least one argument"
    exit 1
fi

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PYTHON_BIN="$(command -v python3 || command -v python || true)"

if [[ -z "$PYTHON_BIN" ]]; then
    echo "Python not found" >&2
    exit 1
fi

BUILDTOOLS_ARGS=(prepare-host-workspace linux)

for arg in "$@"; do
    if [[ "$arg" == "check" ]]; then
        BUILDTOOLS_ARGS+=(--check)
    else
        BUILDTOOLS_ARGS+=("$arg")
    fi
done

exec "$PYTHON_BIN" "$CUR_DIR/buildtools.py" "${BUILDTOOLS_ARGS[@]}"
