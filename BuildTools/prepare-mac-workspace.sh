#!/bin/bash -e

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(command -v python3 || command -v python || true)"

if [[ -z "$PYTHON_BIN" ]]; then
	echo "Python not found" >&2
	exit 1
fi

BUILDTOOLS_ARGS=(prepare-host-workspace macos)
CHECK_ONLY=0
KNOWN_FEATURES=(packages linux web android-arm32 android-arm64 android-x86 toolset dotnet all)

for arg in "$@"; do
	if [[ "$arg" == "check" ]]; then
		CHECK_ONLY=1
		BUILDTOOLS_ARGS+=(--check)
	elif [[ " ${KNOWN_FEATURES[*]} " == *" $arg "* ]]; then
		BUILDTOOLS_ARGS+=("$arg")
	fi
done

while true; do
	if "$PYTHON_BIN" "$CUR_DIR/buildtools.py" "${BUILDTOOLS_ARGS[@]}"; then
		echo "Workspace is ready!"
		exit 0
	fi

	if [[ $CHECK_ONLY -eq 1 ]]; then
		echo "Workspace is not ready"
		exit 1
	fi

	read -p "..."
done
