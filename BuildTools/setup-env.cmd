@echo off
setlocal

set "PYTHON_BIN="
where py >nul 2>nul && set "PYTHON_BIN=py -3"
if not defined PYTHON_BIN where python >nul 2>nul && set "PYTHON_BIN=python"

if not defined PYTHON_BIN (
	echo Python not found
	exit /b 1
)

for /f "delims=" %%i in ('%PYTHON_BIN% "%~dp0buildtools.py" env --shell cmd') do %%i
if errorlevel 1 exit /b 1
%PYTHON_BIN% "%~dp0buildtools.py" env --summary-only
exit /b
