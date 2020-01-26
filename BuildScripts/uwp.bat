echo off

if [%FO_ROOT%] == [] (
	if exist CMakeLists.txt (
		set FO_ROOT=.
	) else (
		set FO_ROOT=..\\
	)
)
if [%FO_BUILD_DEST%] == [] (
	set FO_BUILD_DEST=Build
)

echo Setup environment
pushd %CD%\%FO_ROOT%
set ROOT_FULL_PATH=%CD%
popd

if not exist %FO_BUILD_DEST% mkdir %FO_BUILD_DEST%
pushd %FO_BUILD_DEST%
if not exist UWP mkdir UWP
pushd UWP

echo Build UWP
if not exist x64 mkdir x64
pushd x64
cmake -A x64 -C "%ROOT_FULL_PATH%/BuildScripts/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../../" "%ROOT_FULL_PATH%"
rem cmake --build . --config RelWithDebInfo
popd

rem win32
rem arm
rem arm64

popd
popd
