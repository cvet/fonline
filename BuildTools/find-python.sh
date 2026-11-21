# Sourced by the BuildTools shell wrappers: sets PYTHON_BIN to the first interpreter on PATH that meets
# MIN_PYTHON_VERSION in buildtools.py, preferring python3, then the newest python3.N, then python

PYTHON_BIN=""

for PYTHON_CANDIDATE in python3 $(compgen -c python3. | grep -E '^python3\.[0-9]+$' | sort -t . -k 2,2nr -u) python; do
	if command -v "$PYTHON_CANDIDATE" >/dev/null 2>&1 && "$PYTHON_CANDIDATE" -c 'import sys; sys.exit(int(sys.version_info < (3, 12)))' >/dev/null 2>&1; then
		PYTHON_BIN="$(command -v "$PYTHON_CANDIDATE")"
		break
	fi
done

if [[ -z "$PYTHON_BIN" ]]; then
	echo "Python 3.12 or newer not found on PATH (tried python3, python3.N and python)" >&2
	exit 1
fi
