@echo off
setlocal

python "%~dp0buildtools.py" format-source
exit /b %errorlevel%
