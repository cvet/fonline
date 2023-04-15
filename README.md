# FOnline Engine

> Engine currently in semi-usable state due to heavy refactoring

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://app.codacy.com/project/badge/Grade/25dce96e818d45c48745028fd86c5d99)](https://www.codacy.com/gh/cvet/fonline/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)

## Table of Content

- [Features](#features)
- [Usage](#usage)
  * [Public API](#public-api)
  * [Setup](#setup)
  * [Package dependencies](#package-dependencies)
  * [Statically linked packages](#statically-linked-packages)
  * [Footprint](#footprint)
  * [Tutorial](#tutorial)
- [Work in progress](#work-in-progress)
  * [Roadmap](#roadmap)
  * [Todo list *(generated from source code)*](#todo-list---generated-from-source-code--)
- [Repository structure](#repository-structure)
- [Frequently Asked Questions](frequently-asked-questions)
- [About](#about)

## Features

* Isometric graphics (Fallout 1/2/Tactics or Arcanum -like games)
* Multiplayer mode with authoritative server
* Singleplayer mode (one binary, no network connections)
* Supporting of hexagonal and square map tiling
* Prerendered sprites for environment but with possibility of using 3D models for characters
* Engine core written in C++ (favored C++17 standard)
* Flexible scripting system with varies supporting languages:
  + Mono C#
  + Native C++
  + AngelScript
* Cross-platform with target platforms:
  + Windows
  + Linux
  + macOS
  + iOS
  + Android
  + Web
  + UWP *(PC, Mobile, Xbox)*
* Supporting of following asset file formats:
  + Fallout 1/2
  + Fallout Tactics
  + Arcanum
  + 3D characters in modern graphic formats (FBX)
  + And common graphics formats like PNG or TGA

Important note: *Not all from described above features are already implemented, for additional information look at 'Work in progress' section below*.

## Usage

Engine doesn't targeting to use directly but by as part (submodule) to your own project (git repo).  
Repository contains source code of engine, third-party sources and build tools for composing all this and your stuff into final platform-specific bundles.  
You build your game fully from source, there is no prebuilt binaries, full control over the process.  
*Todo: write about cmake workflow*

### Public API

Documents related to public API:
* [Public API](https://fonline.ru/PUBLIC_API)
* [Scripting API](https://fonline.ru/SCRIPTING_API)

Scripting api automaticly generated for each project individually and api described in [Scripting API](https://fonline.ru/SCRIPTING_API) is only basic.  
See example of extended scripting api at [FOnline TLA Scripting API](https://tla.fonline.ru/SCRIPTING_API).

### Setup

General steps:
* Create your own project repo and link this repo as submodule
* Setup your own .cmake file with your game configuration
* Use main CMakeLists.txt jointly with your .cmake to build game

Reference project:
* FOnline: The Life After https://guthub.com/cvet/fonline-tla

### Package dependencies

Following Linux packages will help us build game for target platforms.  
* Common:  
`clang` `clang-format` `build-essential` `git` `cmake` `python3` `wget` `unzip`
* Building for Linux:  
`libc++-dev` `libc++abi-dev` `binutils-dev` `libx11-dev` `freeglut3-dev` `libssl-dev` `libevent-dev` `libxi-dev` `curl`
* Building for Web:  
`nodejs` `default-jre`
* Building for Android:  
`android-sdk` `openjdk-8-jdk` `ant`

Build scripts download and install following packages:
* [Emscripten](https://emscripten.org) - for building Web apps
* [Android NDK](https://developer.android.com/ndk) - compilation for Android devices

List of tools for Windows operating system *(some optional)*:
* [CMake](https://cmake.org) - utility that helps build program from source on any platform for any platform without much pain
* [Python](https://python.org) - needed for additional game code generation
* [Visual Studio 2022](https://visualstudio.microsoft.com) - IDE for Windows
* [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com) - just build tools without full IDE

List of tools for Mac operating system:
* [CMake](https://cmake.org)
* [Python](https://python.org) - needed for additional game code generation
* [Xcode](https://developer.apple.com/xcode)

Other stuff used in build pipeline:
* [iOS CMake Toolchain](https://github.com/cristeab/ios-cmake)
* [msicreator](https://github.com/jpakkane/msicreator)

### Statically linked packages

These packages included to this repository, will compile and link statically to our binaries.  
They are located in ThirdParty directory.

* ACM by Abel - sound file format reader
* [AngelScript](https://www.angelcode.com/angelscript/) - scripting language
* [Asio](https://think-async.com/Asio/) - networking library
* [backward-cpp](https://github.com/bombela/backward-cpp) - stacktrace obtaining
* [Catch2](https://github.com/catchorg/Catch2) - test framework
* [GLEW](http://glew.sourceforge.net/) - library for binding opengl stuff
* [glslang](https://github.com/KhronosGroup/glslang) - glsl shaders front-end
* [Json](https://github.com/azadkuh/nlohmann_json_release) - json parser
* [PNG](http://www.libpng.org/pub/png/libpng.html) - png image loader
* [SDL2](https://github.com/libsdl-org/SDL) - low level access to audio, input and graphics
* SHA1 & SHA2 generators by Steve Reid and Olivier Gay - hash generators
* [span](https://github.com/tcbrindle/span) - std::span implementation for pre c++20
* [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) - spir-v shaders to other shader languages converter
* [tracy](https://github.com/wolfpld/tracy) - profiler
* [Theora](https://www.theora.org/downloads/) - video library
* [Vorbis](https://xiph.org/vorbis/) - audio library
* [FBX SDK](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2018-1-1) - fbx file format loader
* [{fmt}](https://fmt.dev/latest/index.html) - strings formatting library
* [Dear ImGui](https://github.com/ocornut/imgui) - gui library
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver) - mongo db driver + bson lib
* [Mono](https://www.mono-project.com/) - c# scripting library
* [libogg](https://xiph.org/ogg/) - audio library
* [LibreSSL](https://www.libressl.org/) - library for network transport security
* [unqlite](https://unqlite.org/) - nosql database engine
* [websocketpp](https://github.com/zaphoyd/websocketpp) - websocket asio extension
* [zlib](https://www.zlib.net/) - compression library

### Footprint

Despite on many third-party libraries that consumed by the whole engine one of the main goal is small final footprint of client/singleplayer output.  
Aim to shift most of things of loading specific image/model/sound/ect file formats at pre publishing steps and later use intermediate binary representation for loading resources in runtime as fast as possible and without additional library dependencies.  
This process in terms of fonline engine called `Bakering`.  
Also as you can see all third-party dependencies linked statically to final executable (except one - proprietary libfbxsdk.dll/so for Baker but the last one is not target for small footprint) and this frees up end user from installing additional runtime to play in your game.  
*Todo: write about memory footprint*  
*Todo: write about network footprint*

### Tutorial

Please follow these instructions to understand how to use this engine by design:
* [Tutorial document](https://fonline.ru/TUTORIAL)

## Work in progress

### Roadmap

* [FOnline TLA](https://github.com/cvet/fonline-tla) as demo game [done]
* Code refactoring [85%]
  + Clean up errors handling (error code based + exception based)
  + Preprocessor defines to constants and enums
  + Eliminate raw pointers, use raii and smart pointers for control objects lifetime
  + Fix all warnings from PVS Studio and other static analyzer tools
* AngelScript scripting layer [done]
* Documentation for public API [10%]
* API freezing and continuing development with it's backward compatibility [80%]
* Native C++ scripting layer [20%]
* Improve more unit tests and gain code coverage to at least 80% [10%]
* C#/Mono scripting layer [10%]
* DirectX rendering [done]
* Singleplayer mode [70%]
* Particle system [done]
* Metal rendering for macOS/iOS [1%]

### Todo list *(generated from source code)*

* Common: make entities positioning free in space, without hard-linking to hex
* Common: add third 'up' coordinate to positioning that allow create multidimensional maps
* Common: use smart pointers instead raw
* Common: fix all PVS Studio warnings
* Common: SHA replace to openssl SHA
* Common: wrap fonline code to namespace
* Common: ident_t 8 byte integer
* Common: hash_t 8 byte integer
* Common: tick_t 8 byte integer
* Common: c-style arrays to std::array
* Common: use more noexcept
* Common: use more constexpr
* Common: improve BitReader/BitWriter to better network/disk space utilization
* Common: cast between numeric types via numeric_cast<to>(from)
* Common: improve custom exceptions for every subsustem
* Common: temporary entities, disable writing to data base
* Common: RUNTIME_ASSERT to assert?
* Common: move all return values from out refs to return values as tuple and nodiscard (and then use structuured binding)
* Common: split meanings of int8/char and uint8/byte in code
* Common: improve named enums
* Common: replace depedency from Assimp types (matrix/vector/quaternion/color)
* Common: improve automatic checker of STACK_TRACE_ENTRY/NO_STACK_TRACE_ENTRY in every .cpp function
* Common: pass name to exceptions context args
* Common: split RUNTIME_ASSERT to real uncoverable assert and some kind of runtime error
* Common: recursion guard for EventDispatcher
* Common: improve ptr<> system for leng term pointer observing
* Common: eliminate as much defines as possible
* Common: convert all defines to constants and enums
* Common: remove all id masks after moving to 64-bit hashes
* Common: rework built-in string messages
* Common: optimize copy() to pass placement storage for value
* ServerServiceApp: convert argv from wchar_t** to char**
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: move texcoord offset calculation to gpu
* 3dStuff: merge all bones in one hierarchy and disable offset copying
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: remove unnecessary allocations from 3d
* 3dStuff: fix AtlasType referencing in 3dStuff
* Client: slowly move to stop hex
* Client: synchronize effects showing (for example shot and kill)
* Client: move targs formatting to scripts
* Client: fix soft scroll if critter teleports
* CritterHexView: fidget animation to scripts
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* ResourceManager: why I disable offset adding?
* ServerConnection: automatically reconnect on network failtures
* SparkExtension: improve particles in 2D
* SpriteManager: improve DirectX rendering
* SpriteManager: optimize sprite atlas filling
* SpriteManager: convert FT_ font flags to enum
* SpriteManager: fix FT_CENTERY_ENGINE workaround
* SpriteManager: move fonts stuff to separate module
* SpriteManager: optimize text formatting - cache previous results
* Sprites: MapSprite releasing
* Sprites: : incapsulate all sprite data
* Updater: support restoring file downloading from interrupted position
* Updater: add update file files checking by hashes
* VideoClip: allow video playing in back direction
* VisualParticles: improve particles in 2D
* AngelScriptScriptDict: rework objects in dict comparing (detect opLess/opEqual automatically)
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* DeferredCalls: improve deferred calls
* Entity: improve entity event ExPolicy
* Entity: improve entity event Priority
* Entity: improve entity event OneShot
* Entity: improve entity event Deferred
* EntityProperties: implement Location InitScript
* GeometryHelper: remove hex offset limit
* Log: server logs append not rewrite (with checking of size)
* Log: add timestamps and process id and thread id to file logs
* Log: colorize log texts
* MapLoader: restore supporting of the map old text format
* MsgFiles: pass default to fomsg gets
* Properties: validate property name identifier
* Properties: restore quest variables
* Properties: don't preserve memory for not allocated components in entity
* Properties: pack bool properties to one bit
* Properties: remove friend from PropertiesSerializator and use public Property interface
* PropertiesSerializator: maybe need some optional warning for unknown/wrong properties
* Settings-Include: rework global Quit setting
* Settings-Include: remove hardcoded ResourcesDir in package.py
* Settings-Include: move HeadBone to fo3d settings
* Settings-Include: move LegBones to fo3d settings
* Settings-Include: move resource files control (include/exclude/pack rules) to cmake
* Settings: improve editable entry for arrays
* Application: move all these statics to App class fields
* Rendering-OpenGL: make workarounds for work without ARB_uniform_buffer_object
* Rendering-OpenGL: remove GLEW and bind OpenGL functions manually
* AngelScriptScripting-Template: GetASObjectInfo add detailed info about object
* ClientCritterScriptMethods: improve run particles for 2D animations
* ClientCritterScriptMethods: improve animation callbacks for 2D animations
* MonoScripting-Template: set Mono domain user data
* MonoScripting-Template: get Mono domain user data
* AdminPanel: admin panel network to Asio
* Critter: incapsulate Critter::Talk
* CritterManager: don't remeber but need check (IsPlaneNoTalk)
* EntityManager: load global map critters
* Location: EntranceScriptBindId
* Location: encapsulate Location data
* MapManager: if path finding not be reworked than migrate magic number to scripts
* MapManager: check group
* MapManager: make dynamic path growth and move max value to settings
* Networking: catch exceptions in network servers
* Player: restore automaps
* Player: allow attach many critters to sigle player
* Server: disable look distance caching
* Server: validate player moving path
* Server: validate stop position and place critter in it
* Server: control max size explicitly, add option to property registration
* Server: verify property data from client
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: don't remeber but need check (IsPlaneNoTalk)
* Server: reowrk non-stacked items creation from recursive to loop
* Server: improve ban system
* Server: run network listeners dynamically, without restriction, based on server settings
* ServerDeferredCalls: improve deferred calls
* EffectBaker: pre-compile HLSH shaders with D3DCompile
* EffectBaker: enable auto map bindings
* ImageBaker: swap colors of fo palette once in header
* ImageBaker: move png lib setjmp to exceptions
* Mapper: mapper render iface layer
* ParticleEditor: improve EmitterAttacher (2)
* ParticleEditor: improve ActionSet
* ParticleEditor: DrawSparkObject SPK::EmitterAttacher
* ParticleEditor: DrawSparkObject SPK::ActionSet
* ParticleEditor: DrawSparkObject SPK::SpawnParticlesAction
  
## Repository structure

* [BuildTools](https://github.com/cvet/fonline/tree/master/BuildTools) - scripts for automatical build in command line or any ci/cd system
* [Resources](https://github.com/cvet/fonline/tree/master/Resources) - resources for build applications but not related to code
* [Source](https://github.com/cvet/fonline/tree/master/Source) - fonline engine specific code
* [ThirdParty](https://github.com/cvet/fonline/tree/master/ThirdParty) - external dependencies of engine, included to repository

## Frequently Asked Questions

*Todo: write FAQ*

## About

* Site: [fonline.ru](https://fonline.ru)
* GitHub: [github.com/cvet/fonline](https://github.com/cvet/fonline)
* E-Mail: <cvet@tut.by>
