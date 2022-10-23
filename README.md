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
  * [Roadmap for Visual Studio Code extension](#roadmap-for-visual-studio-code-extension)
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

Following Linux packages will help us build our game for target platforms.  
These packages will automatically installed during workspace preparing (i.e. `prepare-workspace.sh`).
* Common:  
`clang` `clang-format` `build-essential` `git` `cmake` `python3` `wget` `unzip`
* Building for Linux:  
`libc++-dev` `libc++abi-dev` `binutils-dev` `libx11-dev` `freeglut3-dev` `libssl-dev` `libevent-dev` `libxi-dev` `curl`
* Building for Web:  
`nodejs` `default-jre`
* Building for Android:  
`android-sdk` `openjdk-8-jdk` `ant`

Also our build scripts download and install following packages:
* [Emscripten](https://emscripten.org) - for building Web apps
* [Android NDK](https://developer.android.com/ndk) - compilation for Android devices

List of tools for Windows operating system *(some optional)*:
* [CMake](https://cmake.org) - utility that helps build program from source on any platform for any platform without much pain
* [Python](https://python.org) - needed for additional game code generation
* [Visual Studio 2019](https://visualstudio.microsoft.com) - IDE for Windows
* [Build Tools for Visual Studio 2019](https://visualstudio.microsoft.com) - just build tools without full IDE
* [Visual Studio Code](https://code.visualstudio.com) - IDE for Windows with supporting of our engine management
* [WiX Toolset](https://wixtoolset.org) - building installation packages (like .msi)

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

**Near releases:**

* [FOnline TLA](https://github.com/cvet/fonline-tla) as demo game
* Code refactoring
  + Clean up errors handling (error code based + exception based)
  + Preprocessor defines to constants and enums
  + Eliminate raw pointers, use raii and smart pointers for control objects lifetime
  + Fix all warnings from PVS Studio and other static analyzer tools
* Native C++ and AngelScript scripting layers
* Documentation for public API
* API freezing and continuing development with it's backward compatibility

**Futher releases:**

* Visual Studio Code extension *(see separate section below)*
* Improve more unit tests and gain code coverage to at least 80%
* C#/Mono scripting layer
* DirectX rendering with Universal Windows Platform
* Singleplayer mode
* Particle system
* Metal rendering for macOS/iOS

### Roadmap for Visual Studio Code extension

* Integrate mapper for editing .fomap
* Integrate dialog editor for editing .fodlg
* Integrate some property grid for protos editing
* Integrate server
* Integrate client
* Improve viewers for supported graphic formats (frm, spr, png, fofrm and etc)
* Add supporting of AngelScript language (highlight, auto-completion)
* Improve debugging of code (core and scripting)
* Improve debugging of game logic (like run game on this map with these scripts)
* Integrate gui editor for editing .fogui
* Add snippets for common tasks (like create map, create script, create proto)

### Todo list *(generated from source code)*

* Common: rework all commented code during refactoring
* Common: make entities positioning free in space, without hard-linking to hex
* Common: add third 'up' coordinate to positioning that allow create multidimensional maps
* Common: use Common.h as precompiled header
* Common: use smart pointers instead raw
* Common: fix all PVS Studio warnings
* Common: SHA replace to openssl SHA
* Common: add #undef for every local #define
* Common: improve valgrind
* Common: add behaviour for SDL_WINDOW_ALWAYS_ON_TOP
* Common: move defines to const and enums
* Common: don't use rtti/typeid and remove from compilation options?
* Common: wrap fonline code to namespace?
* Common: fix build warnings for all platforms
* Common: enable threating warnings as errors
* Common: id and hash to 8 byte integer
* Common: research about std::filesystem
* Common: compile with -fpedantic
* Common: c-style arrays to std::array
* Common: use more STL (for ... -> auto p = find(begin(v), end(v), val); find_if, begin, end...)
* Common: use par (for_each(par, v, [](int x))
* Common: improve some single standard to initialize objects ({} or ())
* Common: add constness as much as nessesary
* Common: iterator -> const_iterator, auto -> const auto
* Common: use using instead of typedef
* Common: rework unscoped enums to scoped enums
* Common: use more noexcept
* Common: use more constexpr
* Common: improve BitReader/BitWriter to better network/disk space utilization
* Common: organize class members as public, protected, private; methods, fields
* Common: prefer this construction if(auto i = do(); i < 0) i... else i...
* Common: improve std::to_string or fmt::format to string conversions
* Common: cast between numeric types via numeric_cast<to>(from)
* Common: minimize platform specific API (ifdef FO_os, WinApi-Include.h...)
* Common: clang debug builds with sanitiziers
* Common: time ticks to uint64
* Common: improve custom exceptions for every subsustem
* Common: improve particle system based on SPARK engine
* Common: research about Steam integration
* Common: speed up content loading from server
* Common: temporary entities, disable writing to data base
* Common: RUNTIME_ASSERT to assert
* Common: move all return values from out refs to return values as tuple and nodiscard (and then use structuured binding)
* Common: review all SDL_hints.h entries
* Common: fix all warnings (especially under clang) and enable threating warnings as errors
* Common: improve named enums
* Common: split meanings if int8 and char in code
* Common: move from 32 bit hashes to 64 bit
* Common: rename uchar to uint8 and use uint8_t as alias
* Common: rename ushort to uint16 and use uint16_t as alias
* Common: rename uint to uint32 and use uint32_t as alias
* Common: rename char to int8 and use int8_t as alias
* Common: rename short to int16 and use int16_t as alias
* Common: rename int to int32 and use int32_t as alias
* Common: replace depedency from Assimp types (matrix/vector/quaternion/color)
* Common: pass name to exceptions context args
* Common: split RUNTIME_ASSERT to real uncoverable assert and some kind of runtime error
* Common: recursion guard for EventDispatcher
* Common: improve ptr<> system for leng term pointer observing
* Common: fix TRect Width/Height
* Common: eliminate as much defines as possible
* Common: convert all defines to constants and enums
* Common: remove all id masks after moving to 64-bit hashes
* Common: optimize copy() to pass placement storage for value
* Common: apply scripts strack trace
* ServerApp: allow instantiate client in separate thread (fix mt rendering issues)
* ServerServiceApp: convert argv from wchar_t** to char**
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: move texcoord offset calculation to gpu
* 3dStuff: merge all bones in one hierarchy and disable offset copying
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: remove unnecessary allocations from 3d
* 3dStuff: fix AtlasType referencing in 3dStuff
* Client: run updater if resources changed
* Client: synchronize effects showing (for example shot and kill)
* Client: global map critters
* Client: move targs formatting to scripts
* Client: fix soft scroll if critter teleports
* CritterHexView: restore 2D sprite moving
* CritterHexView: expose text on head offset to some settings
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* MapView: optimize, collect separate collection with IsNeedProcess
* MapView: rework smooth item re-appearing before same item still on map
* MapView: optimize lighting rebuilding to skip unvisible lights
* MapView: generate unique entity id
* ResourceManager: why I disable offset adding?
* ServerConnection: automatically reconnect on network failtures
* SpriteManager: restore ShowSpriteBorders
* SpriteManager: improve DirectX rendering
* SpriteManager: maybe restrict fps at 60?
* SpriteManager: optimize sprite atlas filling
* SpriteManager: convert FT_ font flags to enum
* SpriteManager: fix FT_CENTERY_ENGINE workaround
* SpriteManager: move fonts stuff to separate module
* SpriteManager: optimize text formatting - cache previous results
* Sprites: MapSprite releasing
* Sprites: : incapsulate all sprite data
* VisualParticles: improve particles in 2D
* AngelScriptScriptDict: rework objects in dict comparing (detect opLess/opEqual automatically)
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* DeferredCalls: improve deferred calls
* Dialogs: validate script entries, hashes
* Dialogs: verify DlgScriptFunc
* Entity: improve entity event ExPolicy
* Entity: improve entity event Priority
* Entity: improve entity event OneShot
* Entity: improve entity event Deferred
* EntityProperties: implement Location InitScript
* Log: server logs append not rewrite (with checking of size)
* Log: add timestamps and process id and thread id to file logs
* Log: delete \n appendix from WriteLog
* Log: colorize log texts
* MapLoader: restore supporting of the map old text format
* MapLoader: remove mapper specific IsSelected from MapTile
* MsgFiles: pass default to fomsg gets
* Properties: validate property name identifier
* Properties: restore quest variables
* Properties: don't preserve memory for not allocated components in entity
* Properties: pack bool properties to one bit
* Properties: remove friend from PropertiesSerializator and use public Property interface
* ScriptSystem: remove commented code
* Settings-Include: rework global Quit setting
* Settings-Include: move HeadBone to fo3d settings
* Settings-Include: move LegBones to fo3d settings
* Settings-Include: move resource files control (include/exclude/pack rules) to cmake
* Settings: improve editable entry for arrays
* Application: move all these statics to App class fields
* Application: rework sdl_event.text.text
* Rendering-Direct3D: pass additional defines to shaders (passed + internal)
* Rendering-OpenGL: remove GLEW and bind OpenGL functions manually
* Rendering-OpenGL: map all framebuffer ext functions
* Rendering-OpenGL: bind time, random, anim
* Rendering: split ModelBuffer by number of supported bones (1, 5, 10, 20, 35, 54)
* AngelScriptScripting-Template: GetASObjectInfo add detailed info about object
* ClientCritterScriptMethods: handle AbstractItem in Animate
* ClientCritterScriptMethods: call animCallback
* ClientItemScriptMethods: solve recursion in GetMapPos
* CommonGlobalScriptMethods: quad tiles fix (2)
* MapperGlobalScriptMethods: Settings.MapsDir
* MonoScripting-Template: set Mono domain user data
* MonoScripting-Template: get Mono domain user data
* AdminPanel: admin panel network to Asio
* Critter: incapsulate Critter::Talk
* CritterManager: don't remeber but need check (IsPlaneNoTalk)
* EntityManager: load locations -> theirs maps -> critters/items on map -> items in critters/containers
* Location: EntranceScriptBindId
* Location: encapsulate Location data
* MapManager: if path finding not be reworked than migrate magic number to scripts
* MapManager: check group
* Networking: catch exceptions in network servers
* Player: restore automaps
* Player: allow attach many critters to sigle player
* Server: disable look distance caching
* Server: control max size explicitly, add option to property registration
* Server: verify property data from client
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: restore DialogUseResult
* Server: don't remeber but need check (IsPlaneNoTalk)
* Server: improve ban system
* Server: remove history DB system?
* Server: run network listeners dynamically, without restriction, based on server settings
* ServerDeferredCalls: improve deferred calls
* EffectBaker: pre-compile HLSH shaders with D3DCompile
* ImageBaker: swap colors of fo palette once in header
* Mapper: clone entities
* Mapper: clone children
* Mapper: need attention! (3)
* Mapper: map resizing
* Mapper: mapper render iface layer
  
## Repository structure

* [BuildTools](https://github.com/cvet/fonline/tree/master/BuildTools) - scripts for automatical build in command line or any ci/cd system
* [Resources](https://github.com/cvet/fonline/tree/master/Resources) - resources for build applications but not related to code
* [Source](https://github.com/cvet/fonline/tree/master/Source) - fonline engine specific code
* [ThirdParty](https://github.com/cvet/fonline/tree/master/ThirdParty) - external dependencies of engine, included to repository

## Frequently Asked Questions

Following document contains some issues thats give additional information about this engine:
* [FAQ document](https://fonline.ru/FAQ)

## About

* Site: [fonline.ru](https://fonline.ru)
* GitHub: [github.com/cvet/fonline](https://github.com/cvet/fonline)
* E-Mail: <cvet@tut.by>
