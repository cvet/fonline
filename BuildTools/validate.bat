echo off

if [%1] == [win32] (
    set BUILD32=1
    set BUILD64=0
) else if [%1] == [win64] (
    set BUILD32=0
    set BUILD64=1
) else (
    echo Invalid build argument, allowed only win32 or win64
    exit /b 1
)

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
set FO_ROOT=%CD%
popd

if not exist %FO_WORKSPACE% mkdir %FO_WORKSPACE%
pushd %FO_WORKSPACE%

if [%BUILD32%] == [1] (
    echo Build 32-bit binaries
    if not exist "build-win32" mkdir "build-win32"
    pushd "build-win32"
    cmake -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="../output" "%FO_ROOT%"
    cmake --build . --config RelWithDebInfo
    popd
)

if [%BUILD64%] == [1] (
    echo Build 64-bit binaries
    if not exist "build-win64" mkdir "build-win64"
    pushd "build-win64"
    cmake -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="../output" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_EDITOR=1 "%FO_ROOT%"
    cmake --build . --config RelWithDebInfo
    popd
)
