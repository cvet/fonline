@echo off
setlocal

set "PYTHON_BIN="
where py >nul 2>nul && set "PYTHON_BIN=py -3"
if not defined PYTHON_BIN where python >nul 2>nul && set "PYTHON_BIN=python"

if not defined PYTHON_BIN (
	echo Python not found
	exit /b 1
)

%PYTHON_BIN% "%~dp0buildtools.py" toolset %*
exit /b %errorlevel%
