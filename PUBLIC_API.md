# FOnline Engine Public API

> Document under development, do not rely on this API before the global refactoring complete.  
> Estimated finishing date is middle of this year.

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

* fonline-setup.ps1
* validate.sh
* validate.bat
* generate-project.sh
* prepare-workspace.sh
* build.sh
* bake-resources.sh
* package.sh

### Environment variables

* FO_ROOT
* FO_WORKSPACE
* FO_CMAKE_CONTRIBUTION

### CMake options

* FONLINE_VERBOSE_BUILD
* FONLINE_OUTPUT_BINARIES_PATH
* FONLINE_WEB_DEBUG
* FONLINE_BUILD_CLIENT
* FONLINE_BUILD_SERVER
* FONLINE_BUILD_MAPPER
* FONLINE_BUILD_BAKER
* FONLINE_UNIT_TESTS
* FONLINE_CODE_COVERAGE
* FONLINE_CUSTOM_VERSION
* FONLINE_NATIVE_SCRIPTING
* FONLINE_ANGELSCRIPT_SCRIPTING
* FONLINE_MONO_SCRIPTING
* FONLINE_GENERATE_PACKAGING_INFO
* FONLINE_CMAKE_CONTRIBUTION

### CMake contribution

CMake contributions included to main CMakeLists.txt scope.  
So you can use different functionality to affect internal build processes but there is not gurantee that this functionality not broken during development.  
For example you can add source file directly to client lib using target_sources(ClientLib "MySourceFile.cpp") and this may work...

* AddContent pathPattern(s)
* AddResources packName pathPattern(s)
* AddRawResources pathPattern(s)
* AddScriptApi headerPath(s)
* AddNativeIncludeDir dir(s)
* AddNativeSource pathPattern(s)
* AddAngelScriptSource pathPattern(s)
* AddMonoAssembly assembly
* AddMonoCommonReference assembly ref(s)
* AddMonoServerReference assembly ref(s)
* AddMonoClientReference assembly ref(s)
* AddMonoMapperReference assembly ref(s)
* AddMonoCommonSource assembly pathPattern(s)
* AddMonoServerSource assembly pathPattern(s)
* AddMonoClientSource assembly pathPattern(s)
* AddMonoMapperSource assembly pathPattern(s)
* CreateConfig config inheritenConfig
* TweakConfig config option value
* CreatePackage package niceName devName author version
* AddClientToPackage package platform config packType
* AddServerToPackage package platform config packType

### Script API

...link to automaticly generated reference manual...

### Internal file formats

* fonline*.json
* .foitem
* .focr
* .fomap
* .foloc
* .fofrm
* .fo3d
* .fos

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
### Repository structure
### Resources packed formats
### Network protocol
### Packaging information
