@echo off
setlocal

python "%~dp0buildtools.py" build %*
exit /b %errorlevel%
