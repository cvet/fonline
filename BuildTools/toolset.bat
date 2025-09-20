@echo off

call %~dp0\setup-env.cmd

pushd %FO_WORKSPACE%

cd build-win64-toolset
cmake --build . --config Release --target %1 --parallel
if errorlevel 1 exit /b 1

popd
exit /b
