@echo off
setlocal

python "%~dp0shared_buildtools.py" format-source
exit /b %errorlevel%
