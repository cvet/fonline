@echo off
setlocal

python "%~dp0buildtools.py" setup-mono %*
exit /b %errorlevel%
