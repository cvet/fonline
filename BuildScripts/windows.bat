rem Usage:
rem set "FO_SOURCE=<source>" && %FO_SOURCE%\BuildScripts\windows.bat

SET BUILD_DIR=%CD%

IF "%FO_CLEAR%"=="TRUE" RMDIR /S /Q "%BUILD_DIR%\windows"
MKDIR "%BUILD_DIR%\windows"

MKDIR "%BUILD_DIR%\windows\x86"
PUSHD "%BUILD_DIR%\windows\x86"
cmake -G "Visual Studio 14 2015" "%FO_SOURCE%\Source"
POPD

MKDIR "%BUILD_DIR%\windows\x64"
PUSHD "%BUILD_DIR%\windows\x64"
cmake -G "Visual Studio 14 2015 Win64" "%FO_SOURCE%\Source"
POPD

cmake --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target FOnlineServer
cmake --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target Mapper
cmake --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target ASCompiler
cmake --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target FOnline

cmake --build "%BUILD_DIR%\windows\x86" --config RelWithDebInfo --target FOnlineServer
cmake --build "%BUILD_DIR%\windows\x86" --config RelWithDebInfo --target Mapper
cmake --build "%BUILD_DIR%\windows\x86" --config RelWithDebInfo --target ASCompiler
cmake --build "%BUILD_DIR%\windows\x86" --config RelWithDebInfo --target FOnline
