# FOnline Engine : Build tools

## Build scripts

For build just run from repository root one of the following scripts:
* BuildTools\windows.bat - build Windows binaries (both win32 and win64) *(run on Windows only)*
* BuildTools\windows-win32.bat - build Windows binaries (win32 only) *(run on Windows only)*
* BuildTools\windows-win64.bat - build Windows binaries (win64 only) *(run on Windows only)*
* BuildTools/linux.sh - build Linux binaries (unit-tests and code-coverage not included) *(run on Unix platforms)*
* BuildTools/linux-unit-tests.sh - build and run unit tests *(run on Unix platforms)*
* BuildTools/linux-code-coverage.sh - build and run code coverage inspection *(run on Unix platforms)*
* BuildTools/web.sh - build Web binaries (both release and debug) *(run on Unix platforms)*
* BuildTools/web-release.sh - build Web binaries (release only) *(run on Unix platforms)*
* BuildTools/web-debug.sh - build Web binaries (debug only) *(run on Unix platforms)*
* BuildTools/android.sh - build Android binaries (both arm32 and arm64) *(run on Unix platforms)*
* BuildTools/android-arm32.sh - build Android binaries (arm32 only) *(run on Unix platforms)*
* BuildTools/android-arm64.sh - build Android binaries (arm64 only) *(run on Unix platforms)*
* BuildTools/mac.sh - build macOS binaries *(run on macOS only)*
* BuildTools/ios.sh - build iOS binaries (arm64) *(run on macOS only)*

## Build environment variables

Build scripts (sh/bat) may be called both from current directory or repository root (e.g. BuildTools/linux.sh).
Following environment variables optionally may be set before starting build scripts:

#### FO_WORKSPACE *(default: Workspace)*

Path to build directory, where all required configuration (_.sln_/_Makefile_/etc.) and compiled binaries will be stored.
Default behaviour is build in repository in 'Workspace' directory which is already placed to gitignore and not follow to futher commit.

Example: `export FO_WORKSPACE=c:/fonline-workspace`

#### FO_ROOT *(default: . or ../ automatic detection)*

Path to root directory of FOnline repository.
If you try run build script from root (e.g. BuildTools/linux.sh) then current directory taked.
If you try run build script from BuildTools directory (e.g. ./linux.sh) then one level outside directory taked.
This behaviour determined by exition of CMakeLists.txt file in current directory.

Example: `export FO_ROOT=c:/fonline-repo`

#### FO_INSTALL_PACKAGES *(default: 1)*

Automatically install packages needed for build.
Needed sudo permissions.

Example: `export FO_INSTALL_PACKAGES=0`
