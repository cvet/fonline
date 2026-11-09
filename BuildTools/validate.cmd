@echo off
setlocal

call "%~dp0find-python.cmd" || exit /b 1

%PYTHON_BIN% "%~dp0buildtools.py" validate %*
exit /b %errorlevel%
