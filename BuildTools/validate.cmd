@echo off
setlocal

python "%~dp0buildtools.py" validate %*
exit /b %errorlevel%
