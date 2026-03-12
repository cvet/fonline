@echo off
setlocal

python "%~dp0shared_buildtools.py" setup-mono %*
exit /b %errorlevel%
