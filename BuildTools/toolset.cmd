@echo off
setlocal

python "%~dp0buildtools.py" toolset %*
exit /b %errorlevel%
