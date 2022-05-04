# FOnline Engine

> Engine currently in completely unusable state due to heavy refactoring.

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://app.codacy.com/project/badge/Grade/25dce96e818d45c48745028fd86c5d99)](https://www.codacy.com/gh/cvet/fonline/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)

## Table of Content

- [Features](#features)
- [Usage](#usage)
  * [Workflow](#workflow)
    + [Public API](#public-api)
    + [Visual Studio Code](#visual-studio-code)
  * [Setup](#setup)
    + [Package dependencies](#package-dependencies)
    + [Statically linked packages](#statically-linked-packages)
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
* Perendered sprites for environment but with possibility of using 3D models for characters
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
You may work on your game either using shell scripts manually or custom Visual Studio Code extension for simplify these things.  
Shell scripts targeted for work under (at least) Windows 10 / Ubuntu-22.04 / macOS 12.  
You build your game fully from source, there is no prebuilt binaries, full control over the process.

### Workflow

Process of creating your game in two words looks like this:
* Once prepare workspace where all intermediate build files will be stored
* Manage game content and resources
* Build executables from source to platforms that you needed
* Compile scripts (AngelScript and/or Mono; but Native were compiled with built executables)
* Bake all resources (shaders, images and etc) to special formats that will be loaded without overhead by server/client
* Package built executables and baked resources from steps above to final platform specific bundle (zip, msi, app, apk and etc)

#### Public API

Documents related to public API:
* [Public API](https://fonline.ru/PUBLIC_API)
* [Scripting API](https://fonline.ru/SCRIPTING_API)

Also you must understand that scripting api automaticly generated for each project individually and api described in [Scripting API](https://fonline.ru/SCRIPTING_API) is only basic.  
See example of extended scripting api at [FOnline TLA Scripting API](https://tla.fonline.ru/SCRIPTING_API).

#### Visual Studio Code

Engine hosts own Visual Studio Code extension for simplify work with engine stuff.  
In editor go to the Extensions tab and then find and install 'FOnline' extension (it's already available in marketplace).  
More about extension usage you can find in [Tutorial](https://fonline.ru/TUTORIAL) document.

### Setup

General steps:
* Create your own project repo and link this repo as submodule
* Setup your game in cmake extension file (see CMake contribution at [Public API](https://fonline.ru/PUBLIC_API))
* Manage your project with Visual Studio Code + FOnline extension (see Visual Studio Code extension at [Public API](https://fonline.ru/PUBLIC_API))

Reference project:
* FOnline: The Life After https://guthub.com/cvet/fonline-tla

#### Package dependencies

Following Linux packages will help us build our game for target platforms.  
These packages will automatically installed during workspace preparing (i.e. `prepare-workspace.sh`).
* Common:  
`clang` `clang-format` `build-essential` `git` `cmake` `python` `wget` `unzip`
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

#### Statically linked packages

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
* [diStorm3](https://github.com/gdabah/distorm) - library for low level function call hooks
* [PNG](http://www.libpng.org/pub/png/libpng.html) - png image loader
* [SDL2](https://www.libsdl.org/download-2.0.php) - low level access to audio, input and graphics
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

**First release version:**

* [FOnline TLA](https://github.com/cvet/fonline-tla) as demo game
* Code refactoring
  + Clean up errors handling (error code based + exception based)
  + Preprocessor defines to constants and enums
  + Eliminate raw pointers, use raii and smart pointers for control objects lifetime
  + Fix all warnings from PVS Studio and other static analyzer tools
* Native C++ and AngelScript scripting layers
* Documentation for public API
* API freezing and continuing development with it's backward compatibility

**Futher releases:** *(in order of priority)*

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
* Common: split meanings if int8 and char in code
* Common: move from 32 bit hashes to 64 bit
* Common: rename uchar to uint8 and use uint8_t as alias
* Common: rename ushort to uint16 and use uint16_t as alias
* Common: rename uint to uint32 and use uint32_t as alias
* Common: rename char to int8 and use int8_t as alias
* Common: rename short to int16 and use int16_t as alias
* Common: rename int to int32 and use int32_t as alias
* Common: remove max_t
* Common: replace depedency from Assimp types (matrix/vector/quaternion/color)
* Common: auto expand exception parameters to readable state
* Common: recursion guard for EventDispatcher
* Common: improve ptr<> system for leng term pointer observing
* Common: add _hash c-string literal helper
* Common: fix TRect Width/Height
* Common: eliminate as much defines as possible
* Common: convert all defines to constants and enums
* Common: remove all id masks after moving to 64-bit hashes
* Common: remove critter flags
* Common: remove special OTHER_* params
* Common: apply scripts strack trace
* BakerApp: sound and video preprocessing move to baker
* BakerApp: bake prototypes?
* BakerApp: add dialogs verification during baking
* ServerServiceApp: convert argv from wchar_t** to char**
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: GetAnim1/GetAnim2 int to uint return type
* 3dStuff: fix AtlasType referencing in 3dStuff
* Client: handle mouse wheel
* Client: run updater if resources changed
* Client: proto player?
* Client: synchronize effects showing (for example shot and kill)
* Client: global map critters
* Client: move targs formatting to scripts
* Client: fix soft scroll if critter teleports
* CritterHexView: migrate critter on head text moving in scripts
* CritterHexView: do same for 2d animations
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* MapView: rework smooth item re-appearing before same item still on map
* MapView: optimize lighting rebuilding to skip unvisible lights
* ResourceManager: why I disable offset adding?
* ServerConnection: automatically reconnect on network failtures
* SpriteManager: restore texture saving
* SpriteManager: improve DirectX rendering
* SpriteManager: maybe restrict fps at 60?
* SpriteManager: optimize sprite atlas filling
* SpriteManager: improve client rendering brightness
* SpriteManager: move fonts stuff to separate module
* Sprites: MapSprite releasing
* Sprites: : incapsulate all sprite data
* Application: move different renderers to separate modules
* Application: remove GLEW and bind OpenGL functions manually
* Application: map all framebuffer ext functions
* Application: recognize tablet mode for Windows 10
* Application: fix workaround for strange behaviour of button focus
* Application: split ModelBuffer by number of supported bones (1, 5, 10, 20, 35, 54)
* ApplicationHeadless: move different renderers to separate modules
* ApplicationHeadless: implement effect CanBatch
* ApplicationHeadless: app settings
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* Entity: events array may be modified during call, need take it into account here
* Entity: not exception safe, revert ignore with raii
* Entity: improve entity event ExPolicy
* Entity: improve entity event Priority
* Entity: improve entity event OneShot
* Entity: improve entity event Deferred
* EntityProperties: implement Player InitScript
* EntityProperties: implement Location InitScript
* Log: server logs append not rewrite (with checking of size)
* Log: add timestamps and process id and thread id to file logs
* Log: delete \n appendix from WriteLog
* MapLoader: restore supporting of the map old text format
* MapLoader: pass errors vector to MapLoaderException
* MapLoader: remove mapper specific IsSelected from MapTile
* MsgFiles: pass default to fomsg gets
* MsgFiles: move loading to constructors
* Properties: SetValueFromData
* Properties: convert to hstring
* Properties: don't preserve memory for not allocated components in entity
* Properties: pack bool properties to one bit
* Properties: remove friend from PropertiesSerializator and use public Property interface
* Properties: ResolveHash
* ScriptSystem: fill settings to scripts
* ScriptSystem: FindFunc
* Settings-Include: rework global Quit setting
* Settings: improve editable entry for arrays
* StringUtils: make isNumber const
* AngelScriptScripting-Template: MarshalDict
* AngelScriptScripting-Template: MarshalBackScalarDict
* AngelScriptScripting-Template: GetASObjectInfo
* ClientCritterScriptMethods: handle AbstractItem in Animate
* ClientItemScriptMethods: solve recursion in GetMapPos
* ClientItemScriptMethods: need attention!
* CommonGlobalScriptMethods: fix script system
* MapperGlobalScriptMethods: need attention! (6)
* MapperGlobalScriptMethods: Settings.MapsDir
* MonoScripting-Template: set Mono domain user data
* MonoScripting-Template: get Mono domain user data
* ServerCritterScriptMethods: handle AbstractItem in Action
* ServerCritterScriptMethods: handle AbstractItem in Animate
* AdminPanel: admin panel network to Asio
* Critter: rename to IsOwnedByPlayer
* Critter: replace to !IsOwnedByPlayer
* Critter: move Flags to properties
* Critter: incapsulate Critter::Talk
* CritterManager: don't remeber but need check (IsPlaneNoTalk)
* DeferredCalls: improve deferred calls
* Dialogs: check item name on DR_ITEM
* EntityManager: load locations -> theirs maps -> critters/items on map -> items in critters/containers
* Location: encapsulate Location data
* MapManager: need attention!
* MapManager: if path finding not be reworked than migrate magic number to scripts
* MapManager: check group
* Networking: catch exceptions in network servers
* Player: allow attach many critters to sigle player
* Server: move server loop to async processing
* Server: restore settings
* Server: disable look distance caching
* Server: attach critter to player
* Server: control max size explicitly, add option to property registration
* Server: disable send changing field by client to this client
* Server: restore Dialog_UseResult
* Server: don't remeber but need check (IsPlaneNoTalk)
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: remove history DB system?
* Server: run network listeners dynamically, without restriction, based on server settings
* ImageBaker: finish with GLSL to SPIRV to GLSL/HLSL/MSL
* ImageBaker: add supporting of APNG file format
* ImageBaker: swap colors of fo palette once in header
* Mapper: need attention! (21)
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
