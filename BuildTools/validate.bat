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
set FO_ROOT=%CD%
popd

if not exist %FO_WORKSPACE% mkdir %FO_WORKSPACE%
pushd %FO_WORKSPACE%

if [%1] == [win32] (
    echo Build 32-bit client binaries
    if not exist "build-win32" mkdir "build-win32"
    pushd "build-win32"
    cmake -A Win32 -DFONLINE_OUTPUT_PATH="../output" "%FO_ROOT%"
    cmake --build . --config Release
) else if [%1] == [win32-single] (
    echo Build 32-bit singleplayer binaries
    if not exist "build-win32-singleplayer" mkdir "build-win32-singleplayer"
    pushd "build-win32-singleplayer"
    cmake -A Win32 -DFONLINE_OUTPUT_PATH="../output" -DFONLINE_BUILD_CLIENT=0 -DFONLINE_BUILD_SINGLE=1 "%FO_ROOT%"
    cmake --build . --config Release
) else if [%1] == [win64] (
    echo Build 64-bit all binaries
    if not exist "build-win64-full" mkdir "build-win64-full"
    pushd "build-win64-full"
    cmake -A x64 -DFONLINE_OUTPUT_PATH="../output" -DFONLINE_BUILD_SERVER=1 -DFONLINE_BUILD_SINGLE=1 -DFONLINE_BUILD_MAPPER=1 -DFONLINE_BUILD_ASCOMPILER=1 -DFONLINE_BUILD_BAKER=1 "%FO_ROOT%"
    cmake --build . --config Release
) else (
    echo Invalid build argument, allowed only win32 or win64
    exit /b 1
)
