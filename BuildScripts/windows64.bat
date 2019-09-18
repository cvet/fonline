ECHO OFF

IF [%FO_ROOT%] == [] (
	IF EXIST CMakeLists.txt (
		SET FO_ROOT=.
	) else (
		SET FO_ROOT=..\\
	)
)
IF [%FO_BUILD_DEST%] == [] (
	SET FO_BUILD_DEST=Build
)

PUSHD %CD%\%FO_ROOT%
SET ROOT_FULL_PATH=%CD%
POPD

MKDIR %FO_BUILD_DEST%
PUSHD %FO_BUILD_DEST%

MKDIR Windows\x64
PUSHD Windows\x64
cmake -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="../../" "%ROOT_FULL_PATH%"
POPD

cmake --build Windows\x64 --config RelWithDebInfo --target FOnlineServer
cmake --build Windows\x64 --config RelWithDebInfo --target Mapper
cmake --build Windows\x64 --config RelWithDebInfo --target ASCompiler
cmake --build Windows\x64 --config RelWithDebInfo --target FOnline
POPD
