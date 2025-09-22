@echo off

echo Setup environment

if "%FO_PROJECT_ROOT%"=="" set FO_PROJECT_ROOT=.
if "%FO_ENGINE_ROOT%"=="" set FO_ENGINE_ROOT=%~p0..
if "%FO_WORKSPACE%"=="" set FO_WORKSPACE=Workspace
if "%FO_OUTPUT%"=="" set FO_OUTPUT=%FO_WORKSPACE%\output

call :normalize_path "%FO_PROJECT_ROOT%"
set FO_PROJECT_ROOT=%RETVAL%
call :normalize_path "%FO_ENGINE_ROOT%"
set FO_ENGINE_ROOT=%RETVAL%
call :normalize_path "%FO_WORKSPACE%"
set FO_WORKSPACE=%RETVAL%
call :normalize_path "%FO_OUTPUT%"
set FO_OUTPUT=%RETVAL%

set /p FO_DOTNET_RUNTIME=< "%FO_ENGINE_ROOT%\ThirdParty\dotnet-runtime"

echo FO_PROJECT_ROOT=%FO_PROJECT_ROOT%
echo FO_ENGINE_ROOT=%FO_ENGINE_ROOT%
echo FO_WORKSPACE=%FO_WORKSPACE%
echo FO_OUTPUT=%FO_OUTPUT%
echo FO_DOTNET_RUNTIME=%FO_DOTNET_RUNTIME%

exit /b

:normalize_path
    set RETVAL=%~f1
    exit /b
