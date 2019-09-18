# FOnline Engine

[![GitHub](https://github.com/cvet/fonline/workflows/FOnline%20SDK/badge.svg)](https://github.com/cvet/fonline/actions)
[![Jenkins](https://ci.fonline.ru/buildStatus/icon?job=fonline/master)](https://ci.fonline.ru/blue/organizations/jenkins/fonline/activity)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like isometric games for develop/play alone or with friends.

## Features

* OpenGL/ES/WebGL and DirectX *(wip)* rendering
* C++17, AngelScript, C#/Mono and Fallout Star-Trek as scripting languages *(wip)*
* Editor and Server target platforms
  * Windows
  * Linux
  * macOS *(wip)*
* Client target platforms
  * Windows
  * Linux
  * macOS
  * iOS
  * Android
  * Web
  * PS4 *(wip)*
* Online and singleplayer *(wip)* modes
* Supporting of Fallout 1/2/Tactics, Arcanum and other isometric games asset formats
* Supporting of 3d characters in modern graphic formats
* Hexagonal/square map tiling

## Work in progress

Bugs, performance cases and feature requests see at [Issues page](https://github.com/cvet/fonline/issues).

#### Some major issues (that I work currently)

* Code refactoring (see separate section below)
* C++ as native scripting language with additional optional submodules for AngelScript, C# and Fallout Star-Trek SL
* [Multifunctional editor](https://github.com/cvet/fonline/issues/31)
* [Singleplayer mode](https://github.com/cvet/fonline/issues/12)
* [Documentation](https://github.com/cvet/fonline/issues/49)

#### Some minor issues

* [Direct X rendering](https://github.com/cvet/fonline/issues/47)
* [Supporting of UDP](https://github.com/cvet/fonline/issues/14)
* [Exclude FBX SDK from dependencies](https://github.com/cvet/fonline/issues/22)
* [Parallelism for server](https://github.com/cvet/fonline/issues/32)
* [Steam integration](https://github.com/cvet/fonline/issues/38)

#### Code refactoring plans

* Move errors handling model from error code based to exception based
* Eliminate singletons, statics, global functions
* Preprocessor defines to constants and enums
* Eliminate raw pointers, use only smart
* Hide implementation details from headers using abstraction
* Fix all warnings from PVS Studio and other static analyzer tools
* Improve more unit tests and gain code coverage to at least 80%

## Repository structure

* *BuildScripts* - scripts for automatical build in command line or any ci/cd system
* *Resources* - resources for build applications but not related to code
* *SdkPlaceholder* - all this stuff merged with build output in resulted sdk zip
* *Source* - fonline engine specific code
* *SourceTools* - some tools for formatting code or count it
* *ThirdParty* - external dependencies of engine, included to repository

## Clone / Build / Setup

You can build project by sh/bat script or directly use [CMake](https://cmake.org) generator.  
In any way first you must install CMake version equal or higher then 3.10.2.

For build just run from repository root one of the following scripts:
* BuildScripts\windows.bat - build Windows binaries (run on Windows only)
* BuildScripts/linux.sh - build Linux binaries (run on Unix platforms)
* BuildScripts/web.sh - build Web binaries (run on Unix platforms)
* BuildScripts/android.sh - build Android binaries (run on Unix platforms)
* BuildScripts/mac.sh - build macOS binaries (run on macOS only)
* BuildScripts/ios.sh - build iOS binaries (run on macOS only)

All output binaries you can find in Build/Binaries directory.

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
* [rapidyaml](https://github.com/biojppm/rapidyaml) - YAML parser
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
* [mbedTLS](https://tls.mbed.org/) - library for network transport security
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver) - mongo db driver
* [Mono](https://www.mono-project.com/) - c# scripting library
* [libogg](https://xiph.org/ogg/) - audio library
* [openssl](https://www.openssl.org/) - library for network transport security
* [unqlite](https://unqlite.org/) - nosql database engine
* [variant](https://github.com/mapbox/variant) - c++17 std::variant implementation in c++11/14
* [websocketpp](https://github.com/zaphoyd/websocketpp) - websocket asio extension
* [zlib](https://www.zlib.net/) - compression library

## Help and support

* Site: [fonline.ru](https://fonline.ru)
* GitHub: [github.com/cvet/fonline](https://github.com/cvet/fonline)
* E-Mail: <cvet@tut.by>
* Forums: [fodev.net](https://fodev.net)
* Discord: [invite](https://discord.gg/xa6TbqU)
