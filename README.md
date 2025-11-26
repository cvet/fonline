# FOnline Engine

> Engine currently in semi-usable state due to heavy refactoring

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://app.codacy.com/project/badge/Grade/25dce96e818d45c48745028fd86c5d99)](https://www.codacy.com/gh/cvet/fonline/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/cvet/fonline)

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
* Supporting of hexagonal and square map tiling
* Prerendered sprites for environment but with possibility of using 3D models for characters
* Engine core written in modern C++ (up to C++20)
* Flexible scripting system with varies supporting languages:
  + Native C++ coding
  + AngelScript
  + Mono C#
* Cross-platform with target platforms:
  + Windows
  + Linux
  + macOS
  + iOS
  + Android
  + Web
* Supporting of following asset file formats:
  + Fallout 1/2
  + Fallout Tactics
  + Arcanum
  + FBX (3D characters)
  + and common graphics formats like PNG or TGA

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
* FOnline: The Life After https://github.com/cvet/fonline-tla

### Package dependencies

Following Linux packages need to build game for target platforms:
* Common: `clang` `clang-format` `build-essential` `git` `cmake` `python3` `wget` `unzip`
* Building for Linux: `libc++-dev` `libc++abi-dev` `binutils-dev` `libx11-dev` `freeglut3-dev` `libssl-dev` `libevent-dev` `libxi-dev` `curl`
* Building for Web: `nodejs` `default-jre`
* Building for Android: `android-sdk` `openjdk-8-jdk` `ant`

Build scripts will download and install following packages:
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

Other stuff used in the build pipeline:
* [iOS CMake Toolchain](https://github.com/cristeab/ios-cmake)
* [msicreator](https://github.com/jpakkane/msicreator)

SAST tools:
* [PVS-Studio](https://pvs-studio.com/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code

### Statically linked packages

These packages included to this repository, will compile and link statically to our binaries.  
They are located in ThirdParty directory (except dotnet, it's downladed by demand).

* AcmDecoder by Abel - ACM sound format reader
* [AngelScript](https://www.angelcode.com/angelscript/) - scripting language
* [Asio](https://think-async.com/Asio/) - networking library
* [backward-cpp](https://github.com/bombela/backward-cpp) - stacktrace obtaining
* [Catch2](https://github.com/catchorg/Catch2) - test framework
* [GLEW](http://glew.sourceforge.net/) - library for binding opengl stuff
* [glslang](https://github.com/KhronosGroup/glslang) - glsl shaders front-end
* [Json](https://github.com/azadkuh/nlohmann_json_release) - json parser
* [SDL](https://github.com/libsdl-org/SDL) - low level access to audio, input and graphics
* [small_vector](https://github.com/gharveymn/small_vector) - vector with a small buffer optimization
* [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) - spir-v shaders to other shader languages converter
* [tracy](https://github.com/wolfpld/tracy) - profiler
* [Theora](https://www.theora.org/downloads/) - video library
* [Vorbis](https://xiph.org/vorbis/) - audio library
* [Dear ImGui](https://github.com/ocornut/imgui) - gui library
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver) - mongo db driver + bson lib
* [libogg](https://xiph.org/ogg/) - audio library
* [libpng](https://github.com/pnggroup/libpng) - png image loader
* [LibreSSL](https://www.libressl.org/) - library for network transport security
* [rpmalloc](https://github.com/mjansson/rpmalloc) - general purpose memory allocator
* [ufbx](https://github.com/ufbx/ufbx) - fbx file format loader
* [unordered_dense](https://github.com/martinus/unordered_dense) - fast and densely stored hashmap and hashse
* [unqlite](https://unqlite.org/) - nosql database engine
* [websocketpp](https://github.com/zaphoyd/websocketpp) - websocket asio extension
* [zlib](https://www.zlib.net/) - compression library

### Footprint

Despite on many third-party libraries that consumed by the whole engine one of the main goal is small final footprint of client output.  
Aim to shift most of things of loading specific image/model/sound/ect file formats at pre publishing steps and later use intermediate binary representation for loading resources in runtime as fast as possible and without additional library dependencies.  
This process in terms of fonline engine called `Baking`.  
Also as you can see all third-party dependencies linked statically to final executable and this frees up end user from installing additional runtime to play in your game.  
*Todo: write about memory footprint*  
*Todo: write about network footprint*

### Tutorial

Please follow these instructions to understand how to use this engine by design:
* [Tutorial document](https://fonline.ru/TUTORIAL)

## Work in progress

### Roadmap

* [FOnline TLA](https://github.com/cvet/fonline-tla) as demo game [done]
* Code refactoring [95%]
  + Clean up errors handling (error code based + exception based)
  + Preprocessor defines to constants and enums
  + Eliminate raw pointers, use raii and smart pointers for control objects lifetime
  + Fix all warnings from PVS Studio and other static analyzer tools
* AngelScript scripting layer [done]
* Documentation for public API [10%]
* API freezing and continuing development with it's backward compatibility [85%]
* Native C++ scripting layer [20%]
* Improve more unit tests and gain code coverage to at least 80% [15%]
* C#/Mono scripting layer [30%]
* DirectX rendering [done]
* Particle system [done]
* Metal rendering for macOS/iOS [1%]

### Todo list *(generated from source code)*

* Common: export any_t with ExportType
* Common: replace depedency from Assimp types (matrix/vector/quaternion/color)
* Common: recursion guard for EventDispatcher
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: move texcoord offset calculation to gpu
* 3dStuff: merge all bones in one hierarchy and disable offset copying
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: incapsulate model animation callbacks
* Client: refactor critters inventory updating
* Client: fix soft scroll if critter teleports
* Client: make IfaceAnim scriptable object
* Client: move screen fading to scripts
* Client: move screen quake effect to scripts
* ClientConnection: automatically reconnect on network failures
* CritterHexView: fidget animation to scripts
* CritterView: incapsulate AttachedCritters
* DefaultSprites: incapsulate sprite sheet data
* DefaultSprites: Spr.size()
* HexView: incapsulate hex view fileds
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* MapSprite: : incapsulate all sprite data
* ParticleSprites: optimize sprite atlas filling
* RenderTarget: optimize sprite atlas filling
* ResourceManager: why I disable offset adding?
* SpriteManager: optimize sprite atlas filling
* SpriteManager: convert FT_ font flags to enum
* SpriteManager: fix FT_CENTERY_ENGINE workaround
* SpriteManager: incapsulate sprite data
* SpriteManager: move fonts stuff to separate module
* SpriteManager: optimize text formatting - cache previous results
* TextureAtlas: optimize sprite atlas filling
* TextureAtlas: incapsulate texture atlas & atlas space node data
* Updater: support restoring file downloading from interrupted position
* Updater: add update file files checking by hashes
* AngelScriptScriptDict: rework objects in dict comparing (detect opLess/opEqual automatically)
* CacheStorage: add engine hook to allow user to override cache storage
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* ConfigFile: rework ConfigFile entries to string_view
* Entity: improve entity event ExPolicy
* Entity: improve entity event OneShot
* Entity: improve entity event Deferred
* Entity: entity events map key to hstring
* EntityProperties: exclude item properties from engine:
* EntityProperties: exclude critter properties from engine:
* EntityProperties: exclude map properties from engine:
* EntityProperties: implement Location InitScript
* Geometry: make hex position customizable, to allow to add third Z coordinate
* MapLoader: restore supporting of the map old text format
* Properties: validate property name identifier
* Properties: don't preserve memory for not allocated components in entity
* Properties: pack bool properties to one bit
* PropertiesSerializator: maybe need some optional warning for unknown/wrong properties
* Settings-Include: move HeadBone to fo3d settings
* Settings-Include: move LegBones to fo3d settings
* Settings: improve editable entries
* BasicCore: optimize copy() to pass placement storage for value
* BasicCore: improve named enums
* ExceptionHadling: pass name to exceptions context args
* ExtendedTypes: move IRect to irect
* ExtendedTypes: move FRect to frect
* Logging: server logs append not rewrite (with checking of size)
* Logging: colorize log texts
* MemorySystem: fix use after possible move
* StackTrace: improve automatic checker of FO_STACK_TRACE_ENTRY/FO_NO_STACK_TRACE_ENTRY in every .cpp function
* WorkThread: schedule job repeat with last duration?
* Application: move all these statics to App class fields
* Rendering-OpenGL: make workarounds for work without ARB_uniform_buffer_object
* Rendering-OpenGL: remove GLEW and bind OpenGL functions manually
* AngelScriptScripting-Template: GetASObjectInfo add detailed info about object
* ClientCritterScriptMethods: improve run particles for 2D animations
* ClientCritterScriptMethods: improve animation callbacks for 2D animations
* ClientGlobalScriptMethods: improve SetScreenKeyboard
* MonoScripting-Template: set Mono domain user data
* MonoScripting-Template: get Mono domain user data
* ServerMapScriptMethods: notify clients about manual hex block
* Critter: incapsulate Critter data
* EntityManager: load global map critters
* Map: make movable checks without critter removing
* Map: optimize items radius search by checking directly hexes in radius (2)
* MapManager: if path finding not be reworked than migrate magic number to scripts
* Player: allow attach many critters to sigle player
* Server: validate player moving path
* Server: validate stop position and place critter in it
* Server: control max size explicitly, add option to property registration
* Server: verify property data from client
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: run network listeners dynamically, without restriction, based on server settings
* ServerConnection: incapsulate ServerConnection data
* EffectBaker: pre-compile HLSH shaders with D3DCompile
* EffectBaker: enable auto map bindings
* Mapper: fix memleak (2)
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
