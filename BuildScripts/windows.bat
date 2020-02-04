echo off

if [%1] == [win32] (
    set BUILD32=1
    set BUILD64=0
) else if [%1] == [win64] (
    set BUILD32=0
    set BUILD64=1
) else if [%1] == [] (
    set BUILD32=1
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
if [%FO_BUILD_DEST%] == [] (
    set FO_BUILD_DEST=Build
)

echo Setup environment
pushd %CD%\%FO_ROOT%
set ROOT_FULL_PATH=%CD%
popd

if not exist %FO_BUILD_DEST% mkdir %FO_BUILD_DEST%
pushd %FO_BUILD_DEST%
if not exist Windows mkdir Windows
pushd Windows

if [%BUILD32%] == [1] (
    echo Build 32-bit binaries
    if not exist win32 mkdir win32
    pushd win32
    cmake -A Win32 -DFONLINE_OUTPUT_BINARIES_PATH="../../" "%ROOT_FULL_PATH%"
    cmake --build . --config RelWithDebInfo
    popd
)

if [%BUILD64%] == [1] (
    echo Build 64-bit binaries
    if not exist win64 mkdir win64
    pushd win64
    cmake -A x64 -DFONLINE_OUTPUT_BINARIES_PATH="../../" "%ROOT_FULL_PATH%"
    cmake --build . --config RelWithDebInfo
    popd
)

popd
popd
