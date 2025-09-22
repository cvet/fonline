@echo off
setlocal

call "%~dp0\setup-env.cmd"

if "%~3"=="" (
    echo Usage: setup-mono.cmd os arch config
    exit /b 1
)

set TRIPLET=%1.%2.%3

if not exist CLONED (
    if exist runtime (
        echo Remove previous repository
        rmdir /s /q runtime
    )

    echo Clone runtime
    git clone https://github.com/dotnet/runtime.git --depth 1 --branch v9.0.0
    if errorlevel 1 exit /b 1
)

echo . > CLONED

if not exist BUILT_%TRIPLET% (
    echo Build runtime
    pushd runtime
    cmd.exe /c build.cmd -os %1 -arch %2 -c %3 -subset mono.runtime
    popd
    if errorlevel 1 exit /b 1
)

echo . > BUILT_%TRIPLET%

if not exist READY_%TRIPLET% (
    echo Copy runtime
    call :COPY_RUNTIME runtime\artifacts\obj\mono\%TRIPLET%\out output\mono\%TRIPLET%
    if errorlevel 1 exit /b 1
)

echo . > READY_%TRIPLET%

echo Runtime %TRIPLET% is ready!
exit /b

:COPY_RUNTIME
    if exist %1\ (
        echo Copy from %1 to %2
        xcopy %1 %2\ /s /e /y
    ) else (
        echo Files not found: %1
        exit /b 1
    )
