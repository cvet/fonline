# FOnline Engine (version 0.6 WIP, unstable)

[![Build Status](https://ci.fonline.ru/buildStatus/icon?job=fonline/master)](https://ci.fonline.ru/blue/organizations/jenkins/fonline/activity)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like icometric games for develop/play alone or with friends

## About (not actually ready marked as *)

* C++14
* Editor and Server target platforms
  * Windows
  * Linux
  * macOS*
* Client target platforms
  * Windows
  * Linux
  * macOS
  * iOS
  * Android
  * Web
  * PS4*
* Online or singleplayer*
* Supporting of Fallout 1/2/Tactics, Arcanum, Boldur's Gate and other isometric games asset formats
* Supporting of 3d characters in modern graphic formats
* Hexagonal / Square map tiling
* ...write more

## WIP features

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

## Third-party packages

* ACM reader by Abel
* [AngelScript](https://www.angelcode.com/angelscript/)
* [Asio](https://think-async.com/Asio/)
* [Assimp](http://www.assimp.org/)
* [GLEW](http://glew.sourceforge.net/)
* [Json](https://github.com/azadkuh/nlohmann_json_release)
* [diStorm3](https://github.com/gdabah/distorm)
* [PNG](http://www.libpng.org/pub/png/libpng.html)
* [SDL2](https://www.libsdl.org/download-2.0.php)
* SHA1 generator by Steve Reid
* SHA2 generator by Olivier Gay
* [Theora](https://www.theora.org/downloads/)
* [Vorbis](https://xiph.org/vorbis/)
* [cURL](https://curl.haxx.se/)
* [FBX SDK](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2018-1-1)
* [{fmt}](https://fmt.dev/latest/index.html)
* [Dear ImGui](https://github.com/ocornut/imgui)
* [libbson](http://mongoc.org/libbson/current/index.html)
* [mbedTLS](https://tls.mbed.org/)
* [MongoC Driver](https://github.com/mongodb/mongo-c-driver)
* [libogg](https://xiph.org/ogg/)
* [openssl](https://www.openssl.org/)
* [unqlite](https://unqlite.org/)
* [variant](https://github.com/mapbox/variant)
* [websocketpp](https://github.com/zaphoyd/websocketpp)
* [zlib](https://www.zlib.net/)

## Git LFS

Before clone make sure that you install Git LFS
https://git-lfs.github.com/

## Help and support

* [English-speaking community](https://fodev.net)
* [Russian-speaking community](https://fonline.ru)
