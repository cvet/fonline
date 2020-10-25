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

if [%1] == [win32-client] (
    set BUILD_TARGET=FONLINE_BUILD_CLIENT
    set BUILD_ARCH=Win32
) else if [%1] == [win64-client] (
    set BUILD_TARGET=FONLINE_BUILD_CLIENT
    set BUILD_ARCH=x64
) else if [%1] == [uwp-client] (
    set BUILD_TARGET=FONLINE_BUILD_CLIENT
    set BUILD_ARCH=x64
    set BUILD_CACHE=uwp.cache.cmake
) else if [%1] == [win-clang-client] (
    set BUILD_TARGET=FONLINE_BUILD_CLIENT
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=LLVM_v142
) else if [%1] == [win64-server] (
    set BUILD_TARGET=FONLINE_BUILD_SERVER
    set BUILD_ARCH=x64
) else if [%1] == [win-clang-server] (
    set BUILD_TARGET=FONLINE_BUILD_SERVER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=LLVM_v142
) else if [%1] == [win32-single] (
    set BUILD_TARGET=FONLINE_BUILD_SINGLE
    set BUILD_ARCH=Win32
) else if [%1] == [win64-single] (
    set BUILD_TARGET=FONLINE_BUILD_SINGLE
    set BUILD_ARCH=x64
) else if [%1] == [uwp-single] (
    set BUILD_TARGET=FONLINE_BUILD_SINGLE
    set BUILD_ARCH=x64
    set BUILD_CACHE=uwp.cache.cmake
) else if [%1] == [win64-clang-single] (
    set BUILD_TARGET=FONLINE_BUILD_SINGLE
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=LLVM_v142
) else if [%1] == [win64-mapper] (
    set BUILD_TARGET=FONLINE_BUILD_MAPPER
    set BUILD_ARCH=x64
) else if [%1] == [win64-ascompiler] (
    set BUILD_TARGET=FONLINE_BUILD_ASCOMPILER
    set BUILD_ARCH=x64
) else if [%1] == [win-clang-ascompiler] (
    set BUILD_TARGET=FONLINE_BUILD_ASCOMPILER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=LLVM_v142
) else if [%1] == [win64-baker] (
    set BUILD_TARGET=FONLINE_BUILD_BAKER
    set BUILD_ARCH=x64
) else if [%1] == [win-clang-baker] (
    set BUILD_TARGET=FONLINE_BUILD_BAKER
    set BUILD_ARCH=x64
    set BUILD_TOOLSET=LLVM_v142
) else (
    echo Invalid build argument, allowed only win32 or win64
    exit /b 1
)

set BUILD_DIR=validate-%1
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

if [%BUILD_TOOLSET%] != [] (
    cmake -A %BUILD_ARCH% -T %BUILD_TOOLSET% -DCMAKE_BUILD_TYPE=Debug -D%BUILD_TARGET%=1 -DFONLINE_UNIT_TESTS=0 "%FO_ROOT%"
) else if [%BUILD_CACHE%] == [] (
    cmake -A %BUILD_ARCH% -DCMAKE_BUILD_TYPE=Debug -D%BUILD_TARGET%=1 -DFONLINE_UNIT_TESTS=0 "%FO_ROOT%"
) else (
    cmake -A %BUILD_ARCH% -C "%FO_ROOT%\BuildTool\%BUILD_CACHE%" -DCMAKE_BUILD_TYPE=Debug -D%BUILD_TARGET%=1 -DFONLINE_UNIT_TESTS=0 "%FO_ROOT%"
)
cmake --build . --config Debug
