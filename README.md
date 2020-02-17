# FOnline Engine

> Engine currently in unusable state due to heavy refactoring.  
> Estimated finishing date is middle of this year.

[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like isometric games for develop/play alone or with friends.

## Features

* C++17
* Open source under [MIT license](https://github.com/cvet/fonline/blob/master/LICENSE)
* Online mode with authoritative server
* Singleplayer mode *(with or without back run in online mode)*
* Flexible scripting system with varies supporting languages:
  * Native C bindings *(that allows add more languages for work with engine)*
  * Native C++
  * C#/Mono
  * AngelScript *(with supporting of scripts writtlen for old engine versions like r412)*
  * Fallout Star-Trek *(for run original Fallout 1/2 out of the box)*
* Cross-platform with target platforms:
  * Windows
  * Linux
  * macOS
  * iOS
  * Android
  * Web
  * PlayStation
  * UWP *(PC, Mobile, Xbox)*
* Rendering with:
  * OpenGL
  * OpenGLES
  * WebGL
  * DirectX
  * Metal
  * Vulkan
* Supporting of following asset formats:
  * Fallout 1/2
  * Fallout Tactics
  * Arcanum
  * Boldur's Gate
  * 3d characters in modern graphic formats
  * and more...
* Supporting of hexagonal and square map tiling

Important note: *Not all from descripted above features are already implemented, for additional information look at 'Work in progress' section below*.

## Media

Simplest way is:
* [Ask google about "fonline" in images](https://www.google.com/search?q=fonline&tbm=isch)
* [Or ask google about "fonline" in videos](https://www.google.com/search?q=fonline&tbm=vid)
* [Or ask google about "fonline" in common](https://www.google.com/search?q=fonline)

And two videos to who don't like to google:  
<a href="http://www.youtube.com/watch?feature=player_embedded&v=K_a0g-Lbqm0" target="_blank"><img src="http://img.youtube.com/vi/K_a0g-Lbqm0/0.jpg" alt="FOnline History" width="160" height="120" border="0" /></a> <a href="http://www.youtube.com/watch?feature=player_embedded&v=eY5iqW8ssXg" target="_blank"><img src="http://img.youtube.com/vi/eY5iqW8ssXg/0.jpg" alt="Last Frontier" width="160" height="120" border="0" /></a>

## Usage

Repository contains source code of engine, third-party sources and build tools for composing all this stuff into final platform-specific bundles.  
You may work on your game using shell scripts manually but project hosts own extension for Visual Studio Code for simplify these things.  
Shell scripts targeted for work under Windows 10 within WSL2 (with Ubuntu-18.04, other distros not tested but should work).  
Almost all will be work under native Linuxes but some of scripts (Windows system related like BuildTools/windows.sh) must be run only from WSL2 shell.  
So main point of all of this that you build your game fully from source, there is no prebuilt binaries, full control over the process.  
Information about build scripts you can find at BuildTools/README.md.

### Base install

Clone with git this repository or use Chocolatey `choco install fonline`.  
*Todo: provide BuildTools/generate-project.sh to automatically project generation*  
*Todo: write about versioning SemVer https://semver.org and what public API included to itself*

### Windows Subsystem for Linux

*Todo: write more about WSL2*  
*Todo: write why WSL2 but not WSL1*  
Official document about how to install WSL2:  
https://docs.microsoft.com/en-us/windows/wsl/wsl2-install

### Visual Studio Code

Engine hosts own Visual Studio Code extension for simplify work with engine stuff.  
You can download editor at https://code.visualstudio.com or using Chocolatey `choco install vscode`.  
In editor go to the Extensions tab and then find and install 'FOnline' extension (it's already available in marketplace).  
Extension activates automatically when editor finds `fonline*.json` at workspace root.  
*Todo: write about extension features and usage*  
*Todo: add extension to Chocolatey packages*

### CMake

Simply said CMake is an utility that helps build program from source on any platform for any platform without much pain.  
Also it generates project files for your preferred IDE (Visual Studio, VS Code, CLion and etc).  
You can download latest version from https://cmake.org or using Chocolatey `choco install cmake`.

### Other dependencies

Building within Ubuntu scripts automatically install all nessessary stuff but for Windows check following:  
*Todo: Visual Studio 2019 (or build tools)*  
*Todo: Python (2/3?)*  
*Todo: WiX toolset for installers packaging*  
*Todo: NodeJS requirement in VSCode extension development*

## Work in progress

Bugs, performance cases and feature requests you can disscuss at [Issues page](https://github.com/cvet/fonline/issues).

### Roadmap

* Code refactoring *(look at separate section below)*
* Rework scripting system *(add C bindings with optional submodules for C++, AngelScript and C#/Mono)*
* [Singleplayer mode](https://github.com/cvet/fonline/issues/12)
* Improve DirectX/Metal/Vulkan rendering
* Improve supporting of Universal Windows Platform
* [Documentation](https://github.com/cvet/fonline/issues/49)
* API freezing and continuing development with it's backward compatibility
* Adopt Fallout Star-Trek SL for run Fallout on engine out of the box
* Adding supporting of old engine asset formats *(especially r412)*
* Improve supporting of PlayStation

### Code refactoring plans

* Move errors handling model from error code based to exception based
* Eliminate singletons, statics, global functions
* Preprocessor defines to constants and enums
* Eliminate raw pointers, use raii and smart pointers for control objects lifetime
* Hide implementation details from headers using abstraction and pimpl idiom
* Fix all warnings from PVS Studio and other static analyzer tools
* Improve more unit tests and gain code coverage to at least 80%
* Improve more new C++ features like std::array, std::filesystem, std::string_view and etc
* Decrease platform specific code to minimum (we can leave this work to portable C++ or SDL)
* Add constness as much as nessesary

### Todo list *(generated from source code)*

* Common: rework all commented code during refactoring
* Common: make entities positioning free in space, without hard-linking to hex
* Common: add third 'up' coordinate to positioning that allow create multidimensional maps
* Common: use Common.h as precompiled header
* Common: review push_back -> emplace_back
* Common: use smart pointers instead raw
* Common: fix all PVS Studio warnings
* Common: SHA replace to openssl SHA
* Common: add #undef for every local #define
* Common: improve valgrind
* Common: add behaviour for SDL_WINDOW_ALWAYS_ON_TOP
* Common: move defines to const and enums
* Common: don't use rtti and remove from compilation options
* Common: wrap fonline code to namespace
* Common: fix LINK : warning LNK4044: unrecognized option '/INCREMENTAL:NO'; ignored
* Common: fix LINK : warning LNK4044: unrecognized option '/MANIFEST:NO'; ignored
* Common: fix build warnings for all platforms
* Common: enable threating warnings as errors
* Common: sound and video preprocessing move to editor
* Common: research about engine version naming convention (maybe 2019.1/2/3/4?)
* Common: id and hash to 8 byte integer
* Common: add to copyrigths for https://github.com/taka-no-me/android-cmake
* Common: make all depedencies as git submodules?
* Common: research about std::string_view
* Common: research about std::filesystem
* Common: compile with -fpedantic
* Common: c-style arrays to std::array
* Common: use more STL (for ... -> auto p = find(begin(v), end(v), val); find_if, begin, end...)
* Common: use par (for_each(par, v, [](int x))
* Common: improve some single standard to initialize objects ({} or ())
* Common: iterator -> const_iterator, auto -> const auto
* Common: add constness as much as nessesary
* Common: use using instead of typedef
* Common: rework unscoped enums to scoped enums
* Common: use more noexcept
* Common: use more constexpr
* Common: improve BitReader/BitWriter to better network/disk space utilization
* Common: organize class members as public, protected, private; methods, fields
* Common: research c++20 modules
* Common: prefer this construction if(auto i = do(); i < 0) i... else i...
* Common: improve std::to_string or fmt::format to string conversions
* Common: casts between int types via NumericCast<to>()
* Common: minimize platform specific API (ifdef FO_os, WinApi_Include.h...)
* Common: build debug sanitiziers
* Common: time ticks to uint64
* Common: improve custom exceptions for every subsustem
* Common: improve particle system based on SPARK engine
* Common: research about Steam integration
* Common: support of loading r412 content
* Common: speed up content loading from server
* Common: temporary entities, disable writing to data base
* Common: detect OS automatically not from passed constant from build system
* Common: rename uchar to uint8 and use uint8_t as alias
* Common: rename ushort to uint16 and use uint16_t as alias
* Common: rename uint to uint32 and use uint32_t as alias
* Common: rename char to int8 and use int8_t as alias
* Common: split meanings if int8 and char in code
* Common: rename short to int16 and use int16_t as alias
* Common: rename int to int32 and use int32_t as alias
* Common: move from 32 bit hashes to 64 bit
* Common: replace depedency from assimp types (matrix/vector/quaternion/color)
* Common: remove map/vector/set/pair bindings
* Common: rename GenericException to GenericException
* Common: auto expand exception parameters to readable state
* Common: use NonCopyable as default class specifier
* Common: set StaticClass class specifier to all static classes
* Common: recursion guard for EventDispatcher
* Common: improve ptr<> system for leng term pointer observing
* Common: add _hash c-string literal helper
* Common: remove SAFEREL/SAFEDEL/SAFEDELA macro
* Common: move MIN/MAX to std::min/std::max
* Common: eliminate as much defines as possible
* Common: convert all defines to constants and enums
* Common: move WriteData/ReadData to DataWriter/DataReader
* Common: find something from STL instead TwoBitMask
* Common: move NetProperty to more proper place
* ClientApp: fix script system
* EditorApp: improve editor Undo/Redo
* EditorApp: need attention!
* 3dAnimation: add interpolation for tracks more than two
* 3dStuff: add reverse playing of 3d animation
* 3dStuff: process default animations
* 3dStuff: fix AtlasType referencing in 3dStuff
* Client: remove all memory allocations from client loop
* Client: handle mouse wheel
* Client: synchronize effects showing (for example shot and kill)
* Client: need attention! (2)
* Client: fix soft scroll if critter teleports
* Client: add working in IPv6 networks
* Client: rename FOClient to just Client (after reworking server Client to ClientConnection)
* Client: remove client Self singleton
* CritterView: migrate critter on head text moving in scripts
* CritterView: do same for 2d animations
* HexManager: optimize lighting rebuilding to skip unvisible lights
* HexManager: need attention! (2)
* HexManager: move HexManager to MapView?
* Keyboard: merge Keyboard into App::Input and Client/Mapper
* SpriteManager: restore texture saving
* SpriteManager: finish rendering
* SpriteManager: improve DirectX rendering
* SpriteManager: maybe restrict fps at 60?
* SpriteManager: optimize sprite atlas filling
* SpriteManager: improve client rendering brightness
* SpriteManager: move fonts stuff to separate module
* Sprites: exclude sprite cut system?
* Application: move different renderers to separate modules
* Application: map all framebuffer ext functions
* Application: reasearch about std::quick_exit
* Application: split ModelBuffer by number of supported bones (1, 5, 10, 20, 35, 54)
* CacheStorage: store Cache.bin in player local dir for Windows users?
* CacheStorage: add in-memory cache storage and fallback to it if can't create default
* ConfigFile: improve YAML supporting to config file?
* FileSystem: handle apply file writing
* GenericUtils: script handling in ConvertParamValue
* KeyCodes_Include: rename DIK_ to Key or something else
* Log: server logs append not rewrite (with checking of size)
* Log: add timestamps and process id and thread id to file logs
* Log: delete \n appendix from WriteLog
* MapLoader: restore supporting of the map old text format
* MapLoader: pass errors vector to MapLoaderException
* MapLoader: remove mapper specific IsSelected from MapTile
* NetBuffer: allow transferring of any type and add safe transferring of floats
* Properties: fix script system
* Properties: check can get in SetGetCallback
* Properties: rework FONLINE_
* Properties: don't preserve memory for not allocated components in entity
* ProtoManager: remove ProtoManager::ClearProtos
* Script: rework FONLINE_
* Script: fill settings to scripts
* Script: separate interface for calling script events
* Script: rework FONLINE_
* ScriptBind_Include: register new type with automating string - number convertation, exclude GetStrHash
* ScriptBind_Include: const Crit_GetCritters
* ScriptBind_Include: const Npc_GetTalkedPlayers
* ScriptFunctions_Include: rework FONLINE_
* ScriptFunctions_Include: fix script system (4)
* ScriptFunctions_Include: need attention! (3)
* ScriptInvoker: move script invoker to ScriptSystem
* ScriptInvoker: rework FONLINE_
* ScriptInvoker: fix script system (2)
* ScriptInvoker: take Invoker from func
* ScriptInvoker: rework FONLINE_
* ScriptPragmas: rework FONLINE_
* ScriptPragmas: fix script system (2)
* ScriptPragmas: fix pragma parsing as tokens not as strings
* ScriptPragmas: rework FONLINE_
* Settings: exclude server specific settings from client
* Settings: remove VAR_SETTING must stay only constant values
* StringUtils: restore hash parsing
* Testing: restore stack trace dumping in file
* Testing: improve global exceptions handlers for mobile os
* Testing: fix script system
* Testing: review RUNTIME_ASSERT(( <- double braces
* Testing: send client dumps to server
* Timer: timers to std::chrono
* WinApi_Include: move WinApi to separate module because it's give too much garbage in global namespace
* ImageBaker: finish with GLSL to SPIRV to GLSL/HLSL/MSL
* Mapper: need attention! (25)
* Mapper: add standalone Mapper application
* Mapper: rename FOMapper to just Mapper
* Mapper: remove mapper Self singleton
* ModelBaker: fix assimp, exclude fbxsdk
* ModelBaker: return AssimpLogging on models bakering
* AdminPanel: admin panel network to Asio
* Critter: remove static SlotEnabled and SlotDataSendEnabled
* Critter: rework Client class to ClientConnection
* CritterManager: don't remeber but need check (IsPlaneNoTalk)
* DataBase: restore mongodb (linux segfault and linking errors)
* Dialogs: check item name on DR_ITEM
* Location: encapsulate Location data
* Map: rework FOREACH_PROTO_ITEM_LINES (3)
* MapManager: need attention!
* MapManager: if path finding not be reworked than migrate magic number to scripts
* MapManager: check group
* Networking: catch exceptions in network servers
* Server: restore settings (2)
* Server: restore hashes loading
* Server: clients logging may be not thread safe
* Server: disable look distance caching
* Server: remove from game (2)
* Server: control max size explicitly, add option to property registration
* Server: disable send changing field by client to this client
* Server: don't remeber but need check (IsPlaneNoTalk)
* Server: add container properties changing notifications
* Server: make BlockLines changable in runtime
* Server: need attention!
* Server: rename FOServer to just Server
* Server: remove server Self singleton
* Server: synchronize LangPacks
* Server: run network listeners dynamically, without restriction, based on server settings
  
## Repository structure

* *BuildTools* - scripts for automatical build in command line or any ci/cd system
* *Resources* - resources for build applications but not related to code
* *SdkPlaceholder* - all this stuff merged with build output in resulted sdk zip
* *Source* - fonline engine specific code
* *ThirdParty* - external dependencies of engine, included to repository

## Third-party packages

* ACM by Abel - sound file format reader
* [AngelScript](https://www.angelcode.com/angelscript/) - scripting language
* [Asio](https://think-async.com/Asio/) - networking library
* [Assimp](http://www.assimp.org/) - 3d models/animations loading library
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
* [cURL](https://curl.haxx.se/) - transferring data via different network protocols
* [FBX SDK](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2018-1-1) - fbx file format loader
* [{fmt}](https://fmt.dev/latest/index.html) - strings formatting library
* [Dear ImGui](https://github.com/ocornut/imgui) - gui library
* [libbson](http://mongoc.org/libbson/current/index.html) - bson stuff
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver) - mongo db driver
* [Mono](https://www.mono-project.com/) - c# scripting library
* [libogg](https://xiph.org/ogg/) - audio library
* [openssl](https://www.openssl.org/) - library for network transport security
* [unqlite](https://unqlite.org/) - nosql database engine
* [websocketpp](https://github.com/zaphoyd/websocketpp) - websocket asio extension
* [zlib](https://www.zlib.net/) - compression library

## Help and support

* Site: [fonline.ru](https://fonline.ru)
* GitHub: [github.com/cvet/fonline](https://github.com/cvet/fonline)
* E-Mail: <cvet@tut.by>
* Forums: [fodev.net](https://fodev.net)
* Discord: [invite](https://discord.gg/xa6TbqU)
