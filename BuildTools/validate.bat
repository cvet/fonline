echo off

if [%FO_PROJECT_ROOT%] == [] (
    set FO_PROJECT_ROOT=.
)
if [%FO_ENGINE_ROOT%] == [] (
    set FO_ENGINE_ROOT=%~p0/..
)
if [%FO_WORKSPACE%] == [] (
    set FO_WORKSPACE=Workspace
)
if [%FO_OUTPUT%] == [] (
    set FO_OUTPUT=%FO_WORKSPACE%/output
)

echo Setup environment

CALL :NORMALIZEPATH %FO_PROJECT_ROOT%
set FO_PROJECT_ROOT=%RETVAL%
CALL :NORMALIZEPATH %FO_ENGINE_ROOT%
set FO_ENGINE_ROOT=%RETVAL%
CALL :NORMALIZEPATH %FO_WORKSPACE%
set FO_WORKSPACE=%RETVAL%
CALL :NORMALIZEPATH %FO_OUTPUT%
set FO_OUTPUT=%RETVAL%

echo FO_PROJECT_ROOT=%FO_PROJECT_ROOT%
echo FO_ENGINE_ROOT=%FO_ENGINE_ROOT%
echo FO_WORKSPACE=%FO_WORKSPACE%
echo FO_OUTPUT=%FO_OUTPUT%

if not exist %FO_WORKSPACE% mkdir %FO_WORKSPACE%
pushd %FO_WORKSPACE%

if [%1] == [win32-client] (
    set BUILD_TARGET=FO_BUILD_CLIENT
    set BUILD_ARCH=Win32
) else if [%1] == [win64-client] (
    set BUILD_TARGET=FO_BUILD_CLIENT
    set BUILD_ARCH=x64
) else if [%1] == [uwp-client] (
    set BUILD_TARGET=FO_BUILD_CLIENT
    set BUILD_ARCH=x64
    set BUILD_CACHE=uwp.cache.cmake
) else if [%1] == [win64-clang-client] (
    set BUILD_TARGET=FO_BUILD_CLIENT
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else if [%1] == [win64-server] (
    set BUILD_TARGET=FO_BUILD_SERVER
    set BUILD_ARCH=x64
) else if [%1] == [win64-clang-server] (
    set BUILD_TARGET=FO_BUILD_SERVER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else if [%1] == [win32-single] (
    set BUILD_TARGET=FO_BUILD_SINGLE
    set BUILD_ARCH=Win32
) else if [%1] == [win64-single] (
    set BUILD_TARGET=FO_BUILD_SINGLE
    set BUILD_ARCH=x64
) else if [%1] == [uwp-single] (
    set BUILD_TARGET=FO_BUILD_SINGLE
    set BUILD_ARCH=x64
    set BUILD_CACHE=uwp.cache.cmake
) else if [%1] == [win64-clang-single] (
    set BUILD_TARGET=FO_BUILD_SINGLE
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else if [%1] == [win64-editor] (
    set BUILD_TARGET=FO_BUILD_EDITOR
    set BUILD_ARCH=x64
) else if [%1] == [win64-mapper] (
    set BUILD_TARGET=FO_BUILD_MAPPER
    set BUILD_ARCH=x64
) else if [%1] == [win64-ascompiler] (
    set BUILD_TARGET=FO_BUILD_ASCOMPILER
    set BUILD_ARCH=x64
) else if [%1] == [win64-clang-ascompiler] (
    set BUILD_TARGET=FO_BUILD_ASCOMPILER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else if [%1] == [win64-baker] (
    set BUILD_TARGET=FO_BUILD_BAKER
    set BUILD_ARCH=x64
) else if [%1] == [win64-clang-baker] (
    set BUILD_TARGET=FO_BUILD_BAKER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=ClangCL
) else (
    echo Invalid build argument
    exit /b 1
)

set BUILD_DIR=validate-%1
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

if not [%BUILD_TOOLSET%] == [] (
    cmake -A %BUILD_ARCH% -T %BUILD_TOOLSET% -D%BUILD_TARGET%=1 -DFO_UNIT_TESTS=0 "%FO_PROJECT_ROOT%"
) else if [%BUILD_CACHE%] == [] (
    cmake -A %BUILD_ARCH% -D%BUILD_TARGET%=1 -DFO_UNIT_TESTS=0 "%FO_PROJECT_ROOT%"
) else (
    cmake -A %BUILD_ARCH% -C "%FO_ENGINE_ROOT%\BuildTool\%BUILD_CACHE%" -D%BUILD_TARGET%=1 -DFO_UNIT_TESTS=0 "%FO_PROJECT_ROOT%"
)
cmake --build . --config Debug

exit /B

:NORMALIZEPATH
  set RETVAL=%~f1
  exit /B
