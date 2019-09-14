# FOnline Engine : Build scripts

Build scripts (sh/bat) may be called both from current directory or repository root (e.g. BuildScripts/linux.sh).  
Following environment variables optionally may be set before starting build scripts:

### FO_BUILD_DEST *(default: Build)*

Path to build directory, where all required configuration (_.sln_/_Makefile_/etc.) and compiled binaries will be stored.  
Default behaviour is build in repository in 'Build' directory which is already placed to gitignore and not follow to futher commit.

`FO_BUILD_DEST=c:/fonline-build`

### FO_ROOT *(default: . or ../ automatic detection)*

Path to root directory of FOnline repository.  
If you try run build script from root (e.g. BuildScripts/linux.sh) then current directory taked.  
If you try run build script from BuildScripts directory (e.g. ./linux.sh) then one level outside directory taked.  
This behaviour determined by exition of CMakeLists.txt file in current directory.

`FO_ROOT=c:/fonline-repo`

### FO_INSTALL_PACKAGES *(default: 1)*

Automatically install packages needed for build.  
Needed sudo permissions.
 
`FO_INSTALL_PACKAGES=0`
