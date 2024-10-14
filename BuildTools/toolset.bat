echo off

if [%FO_WORKSPACE%] == [] (
    set FO_WORKSPACE=Workspace
)

echo Setup environment

CALL :NORMALIZEPATH %FO_WORKSPACE%
set FO_WORKSPACE=%RETVAL%

echo FO_WORKSPACE=%FO_WORKSPACE%

cd %FO_WORKSPACE%/build-win64-toolset
cmake --build . --config Release --target %1 --parallel

exit /B

:NORMALIZEPATH
  set RETVAL=%~f1
  exit /B
