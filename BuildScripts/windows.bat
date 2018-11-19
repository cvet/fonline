ECHO ON

IF [%FO_ROOT%] == [] ECHO FO_ROOT variable is not set & EXIT /B 1
IF [%FO_BUILD_DEST%] == [] ECHO FO_BUILD_DEST variable is not set & EXIT /B 1

PUSHD %CD%\%FO_ROOT%
SET ROOT_FULL_PATH=%CD%
POPD

MKDIR %FO_BUILD_DEST%
PUSHD %FO_BUILD_DEST%
IF EXIST windows\Client RD windows\Client /S /Q
IF EXIST windows\Server RD windows\Server /S /Q
IF EXIST windows\Mapper RD windows\Mapper /S /Q
IF EXIST windows\ASCompiler RD windows\ASCompiler /S /Q

MKDIR windows\x86
PUSHD windows\x86
cmake -G "Visual Studio 15 2017" "%ROOT_FULL_PATH%\Source"
POPD

MKDIR windows\x64
PUSHD windows\x64
cmake -G "Visual Studio 15 2017 Win64" "%ROOT_FULL_PATH%\Source"
POPD

cmake --build windows\x64 --config RelWithDebInfo --target FOnlineServer
cmake --build windows\x64 --config RelWithDebInfo --target Mapper
cmake --build windows\x64 --config RelWithDebInfo --target ASCompiler
cmake --build windows\x64 --config RelWithDebInfo --target FOnline
cmake --build windows\x86 --config RelWithDebInfo --target FOnlineServer
cmake --build windows\x86 --config RelWithDebInfo --target Mapper
cmake --build windows\x86 --config RelWithDebInfo --target ASCompiler
cmake --build windows\x86 --config RelWithDebInfo --target FOnline
POPD
