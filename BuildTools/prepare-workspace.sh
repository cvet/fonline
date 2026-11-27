#!/bin/bash -e

if [[ -z $1 ]]; then
    echo "Provide at least one argument"
    exit 1
fi

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/find-python.sh"

# Feature names pass through unchanged: buildtools.py owns the vocabulary and rejects names Linux does not support
BUILDTOOLS_ARGS=(prepare-host-workspace linux)
CHECK_ARGS=()

for arg in "$@"; do
    if [[ "$arg" == "check" ]]; then
        CHECK_ARGS=(--check)
    else
        BUILDTOOLS_ARGS+=("$arg")
    fi
done

exec "$PYTHON_BIN" "$CUR_DIR/buildtools.py" "${BUILDTOOLS_ARGS[@]}" "${CHECK_ARGS[@]}"
