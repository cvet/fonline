@echo off
rem Called by the BuildTools cmd wrappers: sets PYTHON_BIN to the first interpreter that meets
rem MIN_PYTHON_VERSION in buildtools.py, preferring the py launcher over python on PATH

set "PYTHON_BIN="
py -3 -c "import sys; sys.exit(int(sys.version_info < (3, 12)))" >nul 2>nul && set "PYTHON_BIN=py -3"
if not defined PYTHON_BIN python -c "import sys; sys.exit(int(sys.version_info < (3, 12)))" >nul 2>nul && set "PYTHON_BIN=python"

if not defined PYTHON_BIN (
	1>&2 echo Python 3.12 or newer not found ^(tried py -3 and python^)
	exit /b 1
)
exit /b 0
