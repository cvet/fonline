# FOnline Engine

[![Build Status](https://ci.fonline.ru/buildStatus/icon?job=fonline/master)](https://ci.fonline.ru/blue/organizations/jenkins/fonline/activity)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like icometric games for develop/play alone or with friends.

## About (not actually ready marked as *)

* C++14 *(not all refactored but wip)*
* OpenGL/ES/WebGL and DirectX *(wip)* rendering
* AngelScript and C# *(wip)* scripting languages
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
* Online or singleplayer *(wip)* modes
* Supporting of Fallout 1/2/Tactics, Arcanum, Boldur's Gate and other isometric games asset formats
* Supporting of 3d characters in modern graphic formats
* Hexagonal/square map tiling
* ...write more

## Milestones

* Multifunctional editor (currently developed in master branch)
* C# as scripting language (Mono)
* Singleplayer mode
* Parallelism for server
* Documentation
* First release with fixed API and further backward comparability

## Repository structure

* *BuildScripts* - scripts for automatical build in command line or any ci/cd system
* *Other* - historical stuff, deprecated and not used
* *Resources* - resources for build applications but not related to code
* *SdkPlaceholder* - all this stuff merged with build output in resulted sdk zip
* *Source* - fonline engine specific code
* *SourceTools* - some tools for formatting code or count it
* *ThirdParty* - external dependecies of engine, included in repository

## Build

Before clone make sure that you install [Git LFS](https://git-lfs.github.com/).\
...write about build with cmake\
...write about make sdk\
...write about how setup own test environment

## Third-party packages

* ACM by Abel - sound file format reader
* [AngelScript](https://www.angelcode.com/angelscript/) - scripting language
* [Asio](https://think-async.com/Asio/) - networking library
* [Assimp](http://www.assimp.org/) - 3d models/animations loading library
* [GLEW](http://glew.sourceforge.net/) - library for binding opengl stuff
* [Json](https://github.com/azadkuh/nlohmann_json_release) - json parser
* [diStorm3](https://github.com/gdabah/distorm) - library for low level function call hooks
* [PNG](http://www.libpng.org/pub/png/libpng.html) - png image loader
* [SDL2](https://www.libsdl.org/download-2.0.php) - low level access to audio, input and graphics
* SHA1 by Steve Reid - hash generator
* SHA2 by Olivier Gay - hash generator
* [Theora](https://www.theora.org/downloads/) - video library
* [Vorbis](https://xiph.org/vorbis/) - audio library
* [cURL](https://curl.haxx.se/) - transferring data via different network protocols
* [FBX SDK](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2018-1-1) - fbx file format loader
* [{fmt}](https://fmt.dev/latest/index.html) - strings formatting library
* [Dear ImGui](https://github.com/ocornut/imgui) - gui library
* [libbson](http://mongoc.org/libbson/current/index.html) - bson stuff
* [mbedTLS](https://tls.mbed.org/) - library for network transport security
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver) - mongo db driver
* [libogg](https://xiph.org/ogg/) - audio library
* [openssl](https://www.openssl.org/) - library for network transport security
* [unqlite](https://unqlite.org/) - nosql database engine
* [variant](https://github.com/mapbox/variant) - c++17 std::variant implementation in c++11/14
* [websocketpp](https://github.com/zaphoyd/websocketpp) - websocket asio extension
* [zlib](https://www.zlib.net/) - compression library

## Help and support

* E-mail cvet@tut.by
* Telegram @aka_cvet
* Skype cvet___
* Phone/WhatsApp/Telegram/Viber +7 981 9028150
* [English-speaking community](https://fodev.net)
* [Russian-speaking community](https://fonline.ru)
* [Discord invite](https://discord.gg/xa6TbqU)
