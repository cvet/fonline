@echo off
setlocal

python "%~dp0shared_buildtools.py" validate %*
exit /b %errorlevel%
