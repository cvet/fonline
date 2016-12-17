rem Usage:
rem set "FO_SOURCE=<source>" && %FO_SOURCE%\BuildScripts\web.bat

SET BUILD_DIR=%CD%

IF "%FO_CLEAR%"=="TRUE" RMDIR /S /Q "%BUILD_DIR%\web"
MKDIR "%BUILD_DIR%\web"

XCOPY "%FO_SOURCE%\BuildScripts\emsdk" "%BUILD_DIR%\web\emscripten" /S /E /Q /I /D
PUSHD "%BUILD_DIR%\web\emscripten"
call emsdk update
call emsdk install latest
call emsdk activate latest
POPD
call emcc -v

rem cmake -G "Visual Studio 14 2015" "%FO_SOURCE%\Source"
rem cmake --build "%BUILD_DIR%\windows\x64" --config RelWithDebInfo --target FOnlineServer
