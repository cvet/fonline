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

`FO_WORKSPACE (default: Workspace)`

Path where all intermediate build files will be stored.  
In most cases you don't need to specify this variable if you run build scripts from one directory.

### CMake options

These options managed by build scripts (described above) automatically but if you use CMake directly then you can tweak their manually.

`FO_VERBOSE_BUILD (default: OFF)`

By default all non-fonline related output are hidden but with enabling this option you will see whole output from CMake.  
Also some of additional information will be printed during configuration.  
This option can help if something goes wrong during CMake configuration processing.

`FO_OUTPUT_PATH (default: *cmake binary dir*)`

Path where resulted binaries will be placed.  
This option allow to redirect output from different CMake build trees to one place.  
I.e. build win/linux/mac/etc binaries in different places but collect output in single place.

`FO_BUILD_CLIENT (default: OFF)`

Produce client binaries.  
Binaries will be placed in `output/Client` directory in separate directory named as `platform-arch-configuration` (i.e. Windows-win64-Debug).  
These binaries later will be consumed by packager.

`FO_BUILD_SERVER (default: OFF)`

Produce server binaries.  
Binaries will be placed in `output/Server` directory in separate directory named as `platform-arch-configuration` (i.e. Linux-x64-Release).
These binaries later will be consumed by packager.

`FO_BUILD_MAPPER (default: OFF)`

Produce mapper binaries.  
Binaries will be placed in `output/Tools` directory.

`FO_BUILD_ASCOMPILER (default: OFF)`

Produce AngelScript compiler.  
Binaries will be placed in `output/Tools` directory and later used for AngelScript scripts compilation.

`FO_BUILD_BAKER (default: OFF)`

Produce baker binaries.  
Binaries will be placed in `output/Tools` directory and later used for baking resources.

`FO_UNIT_TESTS (default: ON)`

Create binaries for unit testing.  
Binaries will be placed in `output/Tests` directory.

`FO_CODE_COVERAGE (default: OFF)`

Create binaries for code coverage calculation.  
Binaries will be placed in `output/Tests` directory.

### CMake contribution

CMake contributions included to main CMakeLists.txt scope.

* SetupGame option value option value...
  + DEV_NAME - name of game in short format, without whitespaces
  + NICE_NAME - representative name of game, any characters allowed
  + AUTHOR_NAME - authoring
  + GAME_VERSION - any string that describe current version of game
  + NATIVE_SCRIPTING - allow native C++ scripting
  + ANGELSCRIPT_SCRIPTING - allow AngelScript scripting
  + MONO_SCRIPTING - allow Mono C# scripting
* AddContent dir(s)
* AddResources packName dir(s)
* AddRawResources dir(s)
* AddNativeIncludeDir dir(s)
* AddNativeSource pathPattern(s)
* AddMonoAssembly assembly
* AddMonoReference assembly target ref(s)
* AddMonoSource assembly target pathPattern(s)
* CreatePackage package config debug
* AddToPackage package binary platform arch packType [customConfig]

### Script API

Because developers can contribute to script API provided document describes only clear engine script API without any contribution.  
Script API documents:
...

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
