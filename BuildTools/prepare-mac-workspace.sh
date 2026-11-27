#!/bin/bash -e

echo "Prepare workspace"

CUR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$CUR_DIR/find-python.sh"

# Feature names pass through unchanged: buildtools.py owns the vocabulary, and macOS accepts none
BUILDTOOLS_ARGS=(prepare-host-workspace macos)
CHECK_ONLY=0

for arg in "$@"; do
	if [[ "$arg" == "check" ]]; then
		CHECK_ONLY=1
	else
		BUILDTOOLS_ARGS+=("$arg")
	fi
done

if [[ $CHECK_ONLY -eq 1 ]]; then
	BUILDTOOLS_ARGS+=(--check)
fi

while true; do
	STATUS=0
	"$PYTHON_BIN" "$CUR_DIR/buildtools.py" "${BUILDTOOLS_ARGS[@]}" || STATUS=$?

	if [[ $STATUS -eq 0 ]]; then
		echo "Workspace is ready!"
		exit 0
	fi

	# Exit status 2 is an argument error, which installing host tools cannot fix
	if [[ $STATUS -eq 2 ]]; then
		exit 2
	fi

	if [[ $CHECK_ONLY -eq 1 ]]; then
		echo "Workspace is not ready"
		exit 1
	fi

	read -p "..."
done
