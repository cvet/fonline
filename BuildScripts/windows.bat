SET CMAKE_DIR=C:\Program Files (x86)\CMake\bin
SET FO_SOURCE=D:\FOnline\FOnlineSource

SET BUILD_DIR=%CD%

IF "%FO_CLEAR%"=="TRUE" RMDIR /S /Q "%BUILD_DIR%\windows"
MKDIR "%BUILD_DIR%\windows"

MKDIR "%BUILD_DIR%\windows\x86"
PUSHD "%BUILD_DIR%\windows\x86"
"%CMAKE_DIR%\cmake.exe" -G "Visual Studio 14 2015" "%FO_SOURCE%\Source"
POPD

MKDIR "%BUILD_DIR%\windows\x64"
PUSHD "%BUILD_DIR%\windows\x64"
"%CMAKE_DIR%\cmake.exe" -G "Visual Studio 14 2015 Win64" "%FO_SOURCE%\Source"
POPD

"%CMAKE_DIR%\cmake.exe" --build "%BUILD_DIR%\windows\x86" --config RelWithDebInfo --target FOnline
"%CMAKE_DIR%\cmake.exe" --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target FOnlineServer
"%CMAKE_DIR%\cmake.exe" --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target Mapper
"%CMAKE_DIR%\cmake.exe" --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target ASCompiler
