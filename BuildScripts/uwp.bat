echo off

if [%FO_ROOT%] == [] (
	if exist CMakeLists.txt (
		set FO_ROOT=.
	) else (
		set FO_ROOT=..\\
	)
)
if [%FO_WORKSPACE%] == [] (
	set FO_WORKSPACE=Workspace
)

echo Setup environment
pushd %CD%\%FO_ROOT%
set ROOT_FULL_PATH=%CD%
popd

if not exist %FO_WORKSPACE% mkdir %FO_WORKSPACE%
pushd %FO_WORKSPACE%

echo Build UWP-x64
if not exist "build-uwp-x64" mkdir "build-uwp-x64"
pushd "build-uwp-x64"
cmake -A x64 -C "%ROOT_FULL_PATH%/BuildScripts/uwp.cache.cmake" -DFONLINE_OUTPUT_BINARIES_PATH="../output" "%ROOT_FULL_PATH%"
rem cmake --build . --config RelWithDebInfo
rem win32
rem arm
rem arm64
