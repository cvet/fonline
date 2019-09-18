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

MKDIR Windows\x86
PUSHD Windows\x86
cmake -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="../../" "%ROOT_FULL_PATH%"
POPD

cmake --build Windows\x86 --config RelWithDebInfo --target FOnlineServer
cmake --build Windows\x86 --config RelWithDebInfo --target Mapper
cmake --build Windows\x86 --config RelWithDebInfo --target ASCompiler
cmake --build Windows\x86 --config RelWithDebInfo --target FOnline
POPD
