# FOnline Engine : Build tools

## Build scripts

For build just run from repository root one of the following scripts:  
*Todo: finalize API and declare here*

## Build environment variables

Build scripts (sh/bat) can be called both from current directory (e.g. `./linux.sh`) or repository root (e.g. `BuildTools/linux.sh`).  
Following environment variables may be set before starting build scripts:

#### FO_ENGINE_ROOT

Path to root directory of FOnline repository.  
If not specified then taked one level outside directory of running script file (i.e. outside `BuildTools`, at repository root).  

*Default: `$(dirname ./script.sh)/../`*  
*Example: `export FO_ENGINE_ROOT=/mnt/d/fonline`*

#### FO_WORKSPACE

Path to directory where all intermediate build files will be stored.  
Default behaviour is build in current directory plus `Workspace`.  

*Default: `$PWD/Workspace`*  
*Example: `export FO_ENGINE_ROOT=/mnt/d/fonline-workspace`*
