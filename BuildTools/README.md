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

## Shared workspace preparation

Shared workspace parts are prepared through `buildtools.py` and wrapped by the platform-specific scripts.

- Linux: `Engine/BuildTools/prepare-workspace.sh`
- macOS: `Engine/BuildTools/prepare-mac-workspace.sh`
- Windows: `Engine/BuildTools/prepare-win-workspace.ps1`

At the moment the shared flow covers:

- `toolset`
- `emscripten`
- `android-ndk`
- `dotnet`

Host prerequisite checks are also available through the main tool:

- `buildtools.py host-check linux`
- `buildtools.py host-check macos`
- `buildtools.py host-check windows`

Host wrapper scripts now delegate to the unified workspace preparation command:

- `buildtools.py prepare-host-workspace linux ...`
- `buildtools.py prepare-host-workspace windows ...`
- `buildtools.py prepare-host-workspace macos ...`

Emscripten version is pinned by `Engine/ThirdParty/emscripten` and installed into `Workspace/emsdk`.

Examples:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten
python3 Engine/BuildTools/buildtools.py prepare-workspace android-ndk dotnet
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset emscripten android-ndk dotnet --check
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux web dotnet
```

## Windows web debug workflow

The local Windows web debug flow uses these shared commands:

- `buildtools.py build web client RelWithDebInfo`
- `buildtools.py package-web-debug`

For an optimized browser build use:

- `buildtools.py build web client Release`

The packaged browser build is emitted into `Workspace/web-debug/LF-Client-LocalTest-Web` and can be served by the generated `web-server.py` helper.
