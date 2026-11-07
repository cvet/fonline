@echo off
setlocal

call "%~dp0find-python.cmd" || exit /b 1

for /f "delims=" %%i in ('%PYTHON_BIN% "%~dp0buildtools.py" env --shell cmd') do %%i
if errorlevel 1 exit /b 1
%PYTHON_BIN% "%~dp0buildtools.py" env --summary-only
exit /b
