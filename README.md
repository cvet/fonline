# FOnline Engine

> Engine currently in completely unusable state due to heavy refactoring.

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://app.codacy.com/project/badge/Grade/25dce96e818d45c48745028fd86c5d99)](https://www.codacy.com/gh/cvet/fonline/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)

## Table of Content

- [Features](#features)
- [Media](#media)
- [Usage](#usage)
  * [Workflow](#workflow)
  * [Public API](#public-api)
  * [Setup](#setup)
    + [Visual Studio Code](#visual-studio-code)
    + [Package dependencies](#package-dependencies)
    + [Statically linked packages](#statically-linked-packages)
  * [Footprint](#footprint)
  * [Tutorial](#tutorial)
- [Work in progress](#work-in-progress)
  * [Roadmap](#roadmap)
  * [Roadmap for Visual Studio Code extension](#roadmap-for-visual-studio-code-extension)
  * [Code refactoring plans](#code-refactoring-plans)
  * [Todo list *(generated from source code)*](#todo-list---generated-from-source-code--)
- [Repository structure](#repository-structure)
- [Frequently Asked Questions](frequently-asked-questions)
- [Help and support](#help-and-support)

## Features

* Isometric graphics (Fallout 1/2/Tactics or Arcanum -like games)
* Multiplayer mode with authoritative server
* Singleplayer mode *(work without distinguish between server and client sides)*
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

## Media

[![https://img.youtube.com/vi/DzCTz7HjuOM/0.jpg](http://img.youtube.com/vi/DzCTz7HjuOM/0.jpg)](http://www.youtube.com/watch?v=DzCTz7HjuOM "FOnline Intro")

## Usage

Repository contains source code of engine, third-party sources and build tools for composing all this stuff into final platform-specific bundles.  
You may work on your game using shell scripts manually but project hosts own extension for Visual Studio Code for simplify these things.  
Shell scripts targeted for work under Windows 10 / Ubuntu-22.04 / macOS 12.  
You build your game fully from source, there is no prebuilt binaries, full control over the process.

### Workflow

Process of creating your game in two words looks like this:
* Once prepare workspace where all intermediate build files will be stored
* Build executables from source to platforms that you needed
* Compile scripts (AngelScript and/or Mono; but Native were compiled with built executables)
* Bake all resources (shaders, images and etc) to special formats that will be loaded super fast by server/client
* Package built executables and baked resources from steps above to final platform specific bundle (zip, msi, app, apk and etc)
* Enjoy your shipped game and iterate development

There are couple of shell scripts that help us to do it:  
* `BuildTools/prepare-workspace.sh` - prepare our linux workspace to futher work (install linux packages, setup emscripten, download android ndk and etc)
* `BuildTools/prepare-win-workspace.ps1` - windows version of prepare workspace, that helps prepare to work
* `BuildTools/prepare-mac-workspace.sh` - mac version of prepare workspace, that helps prepare to work
* `BuildTools/build.sh/bat` - build executable for specific platform
* `BuildTools/validate.sh` and `BuildTools/validate.bat` - that scripts designed for validate that our sources compiling in general; you don't need that scripts and they need for automatic checking of repo consistency and run from ci/cd system like github actions

Scripts can accept additional arguments (`build.sh` for example accept platform for build for) and this information additionaly described in [BuildTools/README.md](https://github.com/cvet/fonline/blob/master/BuildTools/README.md).

### Public API

*Todo: write about versioning SemVer https://semver.org and what public API included to itself*  
Documents related to public API:
* [Public API](https://fonline.ru/PUBLIC_API)
* [Multiplayer Script API](https://fonline.ru/MULTIPLAYER_SCRIPT_API)
* [Singleplayer Script API](https://fonline.ru/SINGLEPLAYER_SCRIPT_API)
* [Mapper Script API](https://fonline.ru/MAPPER_SCRIPT_API)

### Setup

Clone with git this repository.  
Open repository root in Visual Studio Code, install recommended extensions.  
*Todo: write about project generation*

#### Windows Subsystem for Linux

Main point of WSL2 for us that we can run Windows programs from Linux.  
That feature allows unify almost all our build scripts into one environment.  
Recommended Linux distro is [Ununtu-20.04](https://ubuntu.com) on which all build scripts tested.  
You may use other distro but there is no guarantee that it will work out of the box.

#### Visual Studio Code

Engine hosts own Visual Studio Code extension for simplify work with engine stuff.  
In editor go to the Extensions tab and then find and install 'FOnline' extension (it's already available in marketplace).  
Extension activates automatically when editor finds any file that contains `fonline` in name of any file at workspace root.  
More about extension usage you can find in [Tutorial](https://fonline.ru/TUTORIAL) document.

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
* SHA1 by Steve Reid - hash generator
* SHA2 by Olivier Gay - hash generator
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

* [FOnline TLA]((https://github.com/cvet/fonline-tla)) as demo game
* Code refactoring *(see separate section below)*
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

**Research plans:**

* Supporting of PlayStation
* Integration with Unity engine

### Visual Studio Code extension

* Integrate mapper (as javascript module) for editing .fomap
* Integrate dialog editor for editing .fodlg
* Integrate some property grid for protos editing
* Integrate server (as separate process but render ui in editor)
* Integrate client (as javascript module)
* Improve viewers for supported graphic formats (frm, spr, png, fofrm and etc)
* Add supporting of AngelScript language (highlight, auto-completion, compilation)
* Take and tune some of extensions for C# and C++ scripting
* Improve debugging of code (core and scripting)
* Improve debugging of game logic (like run game on this map with these scripts)
* Integrate gui editor for editing .fogui
* Add snippets for common tasks (like create map, create script, create proto)

### Code refactoring plans

* Move errors handling model from error code based to exception based
* Eliminate singletons, statics, global functions
* Preprocessor defines to constants and enums
* Eliminate raw pointers, use raii and smart pointers for control objects lifetime
* Hide implementation details from headers using abstraction and pimpl idiom
* Fix all warnings from PVS Studio and other static analyzer tools
* Improve more new C++ features like std::array, std::filesystem, std::string_view and etc
* Decrease platform specific code to minimum (we can leave this work to portable C++ or SDL)
* C-style casts to C++-style casts
* Add constness as much as necessary

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
* Common: move WriteData/ReadData to DataWriter/DataReader
* Common: fix TRect Width/Height
* Common: eliminate as much defines as possible
* Common: convert all defines to constants and enums
* Common: remove all id masks after moving to 64-bit hashes
* Common: remove critter flags
* Common: remove special OTHER_* params
* BakerApp: sound and video preprocessing move to baker
* BakerApp: bake prototypes?
* BakerApp: add dialogs verification during baking
* ServerApp: fix data racing
* ServerServiceApp: convert argv from wchar_t** to char**
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: GetAnim1/GetAnim2 int to uint return type
* 3dStuff: fix AtlasType referencing in 3dStuff
* Client: handle mouse wheel
* Client: proto player?
* Client: synchronize effects showing (for example shot and kill)
* Client: move targs formatting to scripts
* Client: fix soft scroll if critter teleports
* Client: add working in IPv6 networks
* CritterView: migrate critter on head text moving in scripts
* CritterView: do same for 2d animations
* HexManager: rework smooth item re-appearing before same item still on map
* HexManager: optimize lighting rebuilding to skip unvisible lights
* HexManager: need attention! (3)
* HexManager: move HexManager to MapView?
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* ResourceManager: why I disable offset adding?
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
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* Entity: events array may be modified during call, need take it into account here
* Entity: not exception safe, revert ignore with raii
* Entity: improve entity event ExPolicy
* Entity: improve entity event Priority
* Entity: improve entity event OneShot
* Entity: improve entity event Deferred
* FileSystem: handle apply file writing
* Log: server logs append not rewrite (with checking of size)
* Log: add timestamps and process id and thread id to file logs
* Log: delete \n appendix from WriteLog
* MapLoader: restore supporting of the map old text format
* MapLoader: pass errors vector to MapLoaderException
* MapLoader: remove mapper specific IsSelected from MapTile
* MsgFiles: pass default to fomsg gets
* MsgFiles: move loading to constructors
* Properties: convert to hstring
* Properties: don't preserve memory for not allocated components in entity
* Properties: pack bool properties to one bit
* Properties: remove friend from PropertiesSerializator and use public Property interface
* Properties: ResolveHash
* ScriptSystem: fill settings to scripts
* Settings-Include: rework global Quit setting
* StringUtils: make isNumber const
* Testing: improve global exceptions handlers for mobile os
* Testing: fix script system
* Testing: exclude using of dynamic memory allocation in this module and decrease chance of exception throwing
* Testing: send client dumps to server
* ClientItemScriptMethods: solve recursion in GetMapPos
* ClientItemScriptMethods: need attention!
* CommonGlobalScriptMethods: fix script system
* MapperGlobalScriptMethods: need attention! (4)
* MonoScripting-Template: set Mono domain user data
* MonoScripting-Template: get Mono domain user data
* ServerItemScriptMethods: fix ItemOwnership::ItemContainer recursion
* AdminPanel: admin panel network to Asio
* Critter: rename to IsOwnedByPlayer
* Critter: replace to !IsOwnedByPlayer
* Critter: incapsulate Critter::Talk
* CritterManager: don't remeber but need check (IsPlaneNoTalk)
* Dialogs: check item name on DR_ITEM
* EntityManager: load locations -> theirs maps -> critters/items on map -> items in critters/containers
* Location: encapsulate Location data
* MapManager: if path finding not be reworked than migrate magic number to scripts
* MapManager: check group
* Networking: catch exceptions in network servers
* Player: allow attach many critters to sigle player
* Server: move server loop to async processing
* Server: restore settings (2)
* Server: disable look distance caching
* Server: attach critter to player
* Server: control max size explicitly, add option to property registration
* Server: disable send changing field by client to this client
* Server: don't remeber but need check (IsPlaneNoTalk)
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: remove history DB system?
* Server: run network listeners dynamically, without restriction, based on server settings
* ImageBaker: finish with GLSL to SPIRV to GLSL/HLSL/MSL
* ImageBaker: add supporting of APNG file format
* ImageBaker: swap colors of fo palette once in header
* Mapper: need attention! (24)
* Mapper: mapper render iface layer
  
## Repository structure

* [BuildTools](https://github.com/cvet/fonline/tree/master/BuildTools) - scripts for automatical build in command line or any ci/cd system
* [Resources](https://github.com/cvet/fonline/tree/master/Resources) - resources for build applications but not related to code
* [Source](https://github.com/cvet/fonline/tree/master/Source) - fonline engine specific code
* [ThirdParty](https://github.com/cvet/fonline/tree/master/ThirdParty) - external dependencies of engine, included to repository

## Frequently Asked Questions

Following document contains some issues thats give additional information about this engine:
* [FAQ document](https://fonline.ru/FAQ)

## Help and support

* Site: [fonline.ru](https://fonline.ru)
* GitHub: [github.com/cvet/fonline](https://github.com/cvet/fonline)
* E-Mail: <cvet@tut.by>
