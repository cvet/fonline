# FOnline Engine Public API

> Document under development, do not rely on this API before the global refactoring complete.  
> Estimated finishing date is middle of 2021.

## Table of Content

...generate content...

## About

Public API is convention between engine development and development of games based on it.  
You can rely on backward compatibility of this API and do not afraid of changed behaviours of exposed functionality.  
If some thing is not described neither in included or not included then it's likely in second group of things.

## What is included

In articles below described what is public api include to itself.  
What is not included or just planned to including will described in the end of this document.

### Visual Studio Code extension

...

### Build scripts

* `BuildTools/prepare-workspace.sh` - prepare our linux workspace to futher work (install linux packages, setup emscripten, download android ndk and etc)
* `BuildTools/prepare-win-workspace.ps1` - windows version of prepare workspace, that helps prepare to work
* `BuildTools/prepare-mac-workspace.sh` - mac version of prepare workspace, that helps prepare to work
* `BuildTools/build.sh/bat` - build executable for specific platform
* `BuildTools/toolset.sh/bat` - script to call different commands for manage build pipeline
* `BuildTools/validate.sh` and `BuildTools/validate.bat` - that scripts designed for validate that our sources compiling in general; you don't need that scripts and they need for automatic checking of repo consistency and run from ci/cd system like github actions

Scripts can accept additional arguments (`build.sh` for example accept platform for build for) and this information additionaly described in [BuildTools/README.md](https://github.com/cvet/fonline/blob/master/BuildTools/README.md).  
Other scripts and files in `BuildTools` dir are not part of reliable public API.

### Environment variables

These environment variables affected only on build scripts described above.  
They settled up automatically if you use Visual Studio Code FOnline extension but if you use scripts manually then you must set at least `FO_CMAKE_CONTRIBUTION` before run scripts.

`FO_WORKSPACE (default: Workspace)`

Path where all intermediate build files will be stored.  
In most cases you don't need to specify this variable if you run build scripts from one directory.

`FO_CMAKE_CONTRIBUTION (default: *empty*)`

Path to file with CMake contribution settings.  
Read more at `FONLINE_CMAKE_CONTRIBUTION` below.

### CMake options

These options managed by build scripts (described above) automatically but if you use CMake directly then you can tweak their manually.

`FONLINE_VERBOSE_BUILD (default: NO)`

By default all non-fonline related output are hidden but with enabling this option you will see whole output from CMake.  
Also some of additional information will be printed during configuration.  
This option can help if something goes wrong during CMake configuration processing.

`FONLINE_OUTPUT_PATH (default: *cmake binary dir*)`

Path where resulted binaries will be placed.  
This option allow to redirect output from different CMake build trees to one place.  
I.e. build win/linux/mac/etc binaries in different places but collect output in single place.

`FONLINE_BUILD_CLIENT (default: NO)`

Produce multiplayer client binaries.  
Binaries will be placed in `output/Client` directory in separate directory named as `platform-arch-configuration` (i.e. Windows-win64-Debug).  
These binaries later will be consumed by packager.

`FONLINE_BUILD_SERVER (default: NO)`

Produce multiplayer server binaries.  
Binaries will be placed in `output/Server` directory in separate directory named as `platform-arch-configuration` (i.e. Linux-x64-Release).
These binaries later will be consumed by packager.

`FONLINE_BUILD_SINGLE (default: NO)`

Produce singleplayer binaries.  
Binaries will be placed in `output/Single` directory in separate directory named as `platform-arch-configuration` (i.e. Web-wasm-Release).
These binaries later will be consumed by packager.

`FONLINE_BUILD_MAPPER (default: NO)`

Produce mapper binaries.  
Binaries will be placed in `output/Tools` directory.

`FONLINE_BUILD_ASCOMPILER (default: NO)`

Produce AngelScript compiler.  
Binaries will be placed in `output/Tools` directory and later used for AngelScript scripts compilation.

`FONLINE_BUILD_BAKER (default: NO)`

Produce baker binaries.  
Binaries will be placed in `output/Tools` directory and later used for baking resources.

`FONLINE_UNIT_TESTS (default: YES)`

Create binaries for unit testing.  
Binaries will be placed in `output/Tests` directory.

`FONLINE_CODE_COVERAGE (default: NO)`

Create binaries for code coverage calculation.  
Binaries will be placed in `output/Tests` directory.

`FONLINE_CMAKE_CONTRIBUTION (default: *empty*)`

Path to CMake contribution file where developers can contribute different kind of things like:
* tweak build configuration and game settings
* append scripts for all provided script layers
* append different content and resources to your game

This is main connection point between engine and game based on it.  
So during development engine source stayed untouched and development processed in separate space.  
Also you able to disable unnecessary stuff in your game (like AngelScript supporting for new projects or Mono for old) and produce binaries with zero overhead for this stuff.

`FONLINE_INFO_MARKDOWN_OUTPUT (default: *empty*)`

Path where different infrormational files in markdown format will be stored.  
Actual script API, resources descriptions, package information and etc.  
No output generated if variable is empty.

Full list of output files:
- MULTIPLAYER_SCRIPT_API.md
- SINGLEPLAYER_SCRIPT_API.md
- MAPPER_SCRIPT_API.md

### CMake contribution

CMake contributions included to main CMakeLists.txt scope.

* SetupGame option value option value...
  + DEV_NAME - name of game in short format, without whitespaces
  + NICE_NAME - representative name of game, any characters allowed
  + AUTHOR_NAME - authoring
  + GAME_VERSION - any string that describe current version of game
  + SINGLEPLAYER - set YES if you work on singleplayer game
  + NATIVE_SCRIPTING - allow native C++ scripting
  + ANGELSCRIPT_SCRIPTING - allow AngelScript scripting
  + MONO_SCRIPTING - allow Mono C# scripting
* AddContent dir(s)
* AddResources packName dir(s)
* AddRawResources dir(s)
* AddNativeIncludeDir dir(s)
* AddNativeSource pathPattern(s)
* AddAngelScriptSource pathPattern(s)
* AddMonoAssembly assembly
* AddMonoReference assembly target ref(s)
* AddMonoSource assembly target pathPattern(s)
* CreateConfig config inheritenConfig
* TweakConfig config option value
* CreatePackage package config debug
* AddToPackage package binary platform arch packType [customConfig]

### Script API

Because developers can contribute to script API provided document describes only clear engine script API without any contribution.  
Script API documents:
* [Multiplayer Script API](https://fonline.ru/MULTIPLAYER_SCRIPT_API)
* [Singleplayer Script API](https://fonline.ru/SINGLEPLAYER_SCRIPT_API)
* [Mapper Script API](https://fonline.ru/MAPPER_SCRIPT_API)

### Internal file formats

* fonline*.json
* .foitem
* .focr
* .fomap
* .foloc
* .fofrm
* .fo3d
* .fos
* .fobin
* .fofx
* .focfg
* .fodlg
* .fogui

### External file formats

* .png
* .tga
* .frm
* .rix
* .fbx
* .ogg
...

### Data base storage

...

## What is not included

In this article described what is not included in public API.  
...write about reasons of instability these things...

### Engine core
### CMake tweaks outside provided API

Yeah, you can use different CMake functionality to affect internal build processes but there is not grantee that this functionality not broken during development.  
For example you can add source file directly to client lib using target_sources(ClientLib "MySourceFile.cpp") and this may work long time but some minor update can break this code.  
So if you ready for these surprises then why not.

### Repository structure
### Resources packed formats
### Network protocol
