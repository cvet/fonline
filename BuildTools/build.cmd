@echo off
setlocal

python "%~dp0shared_buildtools.py" build %*
exit /b %errorlevel%
