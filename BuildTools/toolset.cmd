@echo off
setlocal

python "%~dp0shared_buildtools.py" toolset %*
exit /b %errorlevel%
