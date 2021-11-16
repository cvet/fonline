@echo off
setlocal enabledelayedexpansion
REM	ssltest.bat

set ssltest_bin=%1
set ssltest_bin=%ssltest_bin:/=\%
if not exist %ssltest_bin% exit /b 1

set openssl_bin=%2
set openssl_bin=%openssl_bin:/=\%
if not exist %openssl_bin% exit /b 1

%srcdir%\testssl.bat %srcdir%\server.pem %srcdir%\server.pem %srcdir%\ca.pem ^
    %ssltest_bin% %openssl_bin%
if !errorlevel! neq 0 (
	exit /b 1
)

endlocal
