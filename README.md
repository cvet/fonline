# FOnline Engine

> Engine currently in unusable state due to heavy refactoring.  
> Estimated finishing date is middle of this year.

[![GitHub](https://github.com/cvet/fonline/workflows/make-sdk/badge.svg)](https://github.com/cvet/fonline/actions)
[![Codecov](https://codecov.io/gh/cvet/fonline/branch/master/graph/badge.svg)](https://codecov.io/gh/cvet/fonline)
[![Codacy](https://api.codacy.com/project/badge/Grade/6c9c1cddf6ba4b58bfa94c729a73f315)](https://www.codacy.com/app/cvet/fonline?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=cvet/fonline&amp;utm_campaign=Badge_Grade)

## Goal

Friendly engine for fallout-like isometric games for develop/play alone or with friends.

## Features

* Open Source under [MIT license](https://github.com/cvet/fonline/blob/master/LICENSE)
* OpenGL/ES/WebGL and DirectX rendering
* C++17, AngelScript, C#/Mono and Fallout Star-Trek as scripting languages
* Target platforms
  * Windows
  * Linux
  * macOS
  * iOS
  * Android
  * Web
  * PlayStation
* Online and singleplayer modes
* Supporting of Fallout 1/2/Tactics, Arcanum and other isometric games asset formats
* Supporting of 3d characters in modern graphic formats
* Hexagonal/square map tiling

## Media

Simplest way is:
* [Ask google about "fonline" in images](https://www.google.com/search?q=fonline&tbm=isch)
* [Or ask google about "fonline" in videos](https://www.google.com/search?q=fonline&tbm=vid)
* [Or ask google about "fonline" in common](https://www.google.com/search?q=fonline)

And two videos to who don't like to google:  
<a href="http://www.youtube.com/watch?feature=player_embedded&v=K_a0g-Lbqm0" target="_blank"><img src="http://img.youtube.com/vi/K_a0g-Lbqm0/0.jpg" alt="FOnline History" width="160" height="120" border="0" /></a> <a href="http://www.youtube.com/watch?feature=player_embedded&v=eY5iqW8ssXg" target="_blank"><img src="http://img.youtube.com/vi/eY5iqW8ssXg/0.jpg" alt="Last Frontier" width="160" height="120" border="0" /></a>

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
* [Parallelism where it needed](https://github.com/cvet/fonline/issues/32)
* [Steam integration](https://github.com/cvet/fonline/issues/38)
* YAML as main formal for all text assets

#### Code refactoring plans

* Move errors handling model from error code based to exception based
* Eliminate singletons, statics, global functions
* Preprocessor defines to constants and enums
* Eliminate raw pointers, use raii and smart pointers for control objects lifetime
* Hide implementation details from headers using abstraction and pimpl idiom
* Fix all warnings from PVS Studio and other static analyzer tools
* Improve more unit tests and gain code coverage to at least 80%
* Improve more new C++ features like std::array, std::filesystem, std::string_view and etc
* Eliminate all preprocessor defines related to FONLINE_* (CLIENT, SERVER, EDITOR)
* In general decrease platform specific code to minimum (we can leave this work to portable C++ or SDL)

#### Todo list *(generated from source code)*

* !!! ([1](https://github.com/cvet/fonline/blob/master/Source/Applications/EditorApp.cpp#L213), [2](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L6100), [3](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L4311), [4](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L4432), [5](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L192), [6](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L856), [7](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2276), [8](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2310), [9](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2332), [10](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2356), [11](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2661), [12](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3317), [13](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3355), [14](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3364), [15](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3434), [16](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3448), [17](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3463), [18](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3506), [19](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3587), [20](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3600), [21](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3614), [22](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3979), [23](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4024), [24](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4524), [25](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4810), [26](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4845), [27](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4922), [28](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4945), [29](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L230))
* add interpolation for tracks more than two ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dAnimation.cpp#L420))
* Reverse play ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dStuff.cpp#L2184))
* process default animations ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dStuff.cpp#L2398))
* Synchronize effects showing ([1](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L3375))
* rewrite ([1](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L3907))
* Clean up client 0 and -1 item ids ([1](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L4973))
* Migrate critter on head text moving in scripts ([1](https://github.com/cvet/fonline/blob/master/Source/Client/CritterView.cpp#L617))
* do same for 2d animations ([1](https://github.com/cvet/fonline/blob/master/Source/Client/CritterView.cpp#L974))
* Optimize lighting rebuilding to skip unvisible lights ([1](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L1878))
* Brightness ([1](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L59))
* GLSL -> SPIRV ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L42))
* DirectX rendering ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L43))
* 60 fps at target ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L44))
* add pragma once everywhere ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L45))
* split sources to Server/Client/Other(? Mapper)/Common/ThirdParty(with ReadMe of versions)/Applications ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L46))
* in headers only Common.h other in cpp ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L47))
* review push_back -> emplace_back ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L48))
* use smart pointers instead raw ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L49))
* PVS Studio fix & Codacy fix ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L50))
* entity protos accessors ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L51))
* SHA replace to openssl SHA ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L52))
* remove mbedTLS use OpenSSL ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L53))
* add static hash https://gist.github.com/Lee-R/3839813 ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L54))
* use pimpl https://github.com/oliora/samples/blob/master/spimpl.h ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L55))
* AdminTool to separate file ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L56))
* review RUNTIME_ASSERT( ( ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L57))
* review add_definitions -> set_target_properties ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L58))
* review local #define -> #undef ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L59))
* make daemon/service to separate apps ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L60))
* add TestingApp.cpp? CodeCoverageApp? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L61))
* Linux 32 bit builds? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L62))
* Docker for: linux/android/web/mac/windows build, docker with preinstalled linux server/editor ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L63))
* improve valgrind ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L64))
* do something with CACHE STRING "" FORCE ) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L65))
* add behaviour for SDL_WINDOW_ALWAYS_ON_TOP ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L66))
* stacktrace in Assert (with Copy button?) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L67))
* defines to const ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L68))
* add to cmake message STATUS ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L69))
* improve YAML ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L70))
* suspend all threads on Assertation ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L71))
* fix format source under linux ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L72))
* Renderer -> OpenGL, Direct3D, Null ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L73))
* exclude singletons ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L74))
* copy source to server/editor binaries? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L75))
* CMake ServerLib/ClientLib/... ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L76))
* exclude FO_NO_GRAPHIC, FONLINE_SERVER/CLIENT/EDITOR ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L77))
* ProtoMap -> ClientProtoMap, ServerProtoMap ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L78))
* HexManager -> MapView ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L79), [2](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L101))
* save dump files by Save button?; copied full report; crash report writted to log anyway ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L80))
* server logs append not rewrite with checking of size ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L81))
* add timestamps and process id and thread id to file logs ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L82))
* /GR - no rtti /EHsc - disable ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L83))
* update formatting tool + fix run on linux ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L84))
* remove using std, always use explicit std:: ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L85))
* store Cache.bin in player Local ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L86))
* clean up repository from not used files (third-party docs, bins, Documentation, Other) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L87))
* Jenkinsfile fix kubernetes, add to README.md about labels (or move to cross-compile) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L88))
* Mac cross-compile https://github.com/tpoechtrager/osxcross ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L89))
* Windows cross-compile https://arrayfire.com/cross-compile-to-windows-from-linux/ ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L90))
* wrap fonline code to namespace ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L91))
* fix readme.md newlines for site ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L92))
* use precompiled headers ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L93))
* LINK : warning LNK4044: unrecognized option '/INCREMENTAL:NO'; ignored ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L94))
* LINK : warning LNK4044: unrecognized option '/MANIFEST:NO'; ignored ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L95))
* fix build warnings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L96))
* FO_GCC -> FO_GCC_CLANG ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L97))
* CMAKE_VS_PLATFORM_TOOLSET need? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L98))
* refactor Settings params, make it const ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L99))
* all define -> undef ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L100))
* SpriteManager - split loaders ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L102))
* "Acm" "Ogg" "Vorbis" "Theora" "${PNG16}" to Editor ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L103))
* upgrade NDK, include NDK/JDK to SDK ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L104))
* thread emulation in main loop? Or move threading to server-only ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L105))
* engine version naming convention 2019.1/2/3/4 ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L106))
* IniFile to YamlFile ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L107))
* id and hash to 8 byte long ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L108))
* audio loader with variety formats, then encode to raw wav's ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L109))
* ServiceLocator - Logger, Randomizer, AudioManager, ProtoManager, FileManager, Settings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L110))
* add to copyrigths https://github.com/taka-no-me/android-cmake ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L111))
* write about sudo on build ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L112))
* LLVM as main compiler ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L113))
* make all depedencies as git submodules ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L114))
* TryCopy/TryMove files? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L115))
* fix FO_VERSION collisions ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L116))
* erase add_include, add add include for targets + include to root third-party? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L117))
* or maybe copy all nessesary headers during cmake configuration in separate dir ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L118))
* own std::variant ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L119))
* check std::string_view ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L120))
* check std::filesystem ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L121))
* Renderers - OpenGL, OpenGL ES / WebGL, DirectX, Metal, Vulkan ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L122))
* compile with -fpedantic (and use clang for all!) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L123))
* fo_exception -> std::runtime_error? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L124))
* 'Path' class as filesystem path representation ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L125))
* look at std::filesystem ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L126))
* separate Editor Local/Global settings and Project settings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L127))
* editor Undo/Redo ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L128))
* c-style arrays to std::array ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L129))
* THREAD -> thread_local ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L130))
* thread -> future ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L131))
* CMake verbose output FONLINE_* and FO_* variables separately ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L132))
* return MongoDB to Linux server ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L133))
* id to uint64 uid ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L134))
* hash to uint64 ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L135))
* add standalone Mapper application ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L136))
* size_t to auto, and use more auto in general ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L137))
* Thread::Sleep(0) -> this_thread::yield() ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L138))
* use more STL (for ... -> auto p = find(begin(v), end(v), val); find_if, begin, end...) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L139))
* par (for_each(par, v, [](int x)) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L140))
* some single standard to initialize objects ({} or ()) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L141))
* iterator -> const_iterator, auto -> const auto ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L142))
* typedef -> using ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L143))
* unscoped enums to scoped enums ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L144))
* disable objects moving/copying where it's not necessary ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L145))
* declare class ctors/dtros/copy/move methods; structs only for pod without ctors/dtors/etc ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L146))
* use noexcept ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L147))
* use constexpr ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L148))
* WriteData/ReadData to BitReader/BitWriter ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L149))
* use clang-format ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L150))
* organize class members as public, protected, private; methods, fields ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L151))
* revert Log and Randomizer classes ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L152))
* improve exception safety (https://en.wikipedia.org/wiki/Exception_safety) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L153))
* c++20 modules ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L154))
* eliminate volatile, replace to atomic ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L155))
* Visual Studio toolset - clang-cl ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L156))
* if(auto i = do(); i < 0) i... else i... ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L157))
* use std::to_string ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L158))
* casts between int types via NumericCast<to>() ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L159))
* timers to std::chrono ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L160))
* minimize platform specific API (ifdef FO_os, WinApi_Include.h...) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L161))
* AdminPanel network to Asio ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L162))
* move Common.h to precompiled headers (don't forgot about FONLINE_* defines) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L163))
* build debug sanitiziers ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L164))
* time ticks to uint64 ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L165))
* delete LF from write log ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L166))
* improve custom exceptions for every subsustem ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L167))
* use std::quick_exit ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L168))
* fix name char/short/int to portable 8/16/32 bits (maybe i8/i16/i32/i64/ui8/ui16/ui32/ui64)? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L169))
* auto expand exceptions fo_exception("message", var1, var2...) -> "message (var1, var2)" ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L170))
* rename usings like StrUIntMap to string_uint_map ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L171))
* ImageBaker cache Spr files ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L172))
* detect OS automatically not from passed constant from build system ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L181))
* rename uchar to uint8 and use uint8_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L243))
* rename ushort to uint16 and use uint16_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L244))
* rename uint to uint32 and use uint32_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L245))
* rename char to int8 and use int8_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L246))
* split meanings if int8 and char in code ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L247))
* rename short to int16 and use int16_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L248))
* rename int to int32 and use int32_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L249))
* move from 32 bit hashes to 64 bit ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L250))
* remove map/vector/set/pair bindings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L294))
* use NonCopyable as default class specifier ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L391))
* set StaticClass class specifier to all static classes ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L413))
* recursion guard ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L499))
* improve ptr<> system for leng term pointer observing ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L511))
* add _hash c-string literal helper ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L592))
* remove SAFEREL/SAFEDEL/SAFEDELA macro ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L613))
* MIN/MAX to std::min/std::max ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L633))
* eliminate as much defines as possible ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L664))
* convert all defines to constants and enums ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L665))
* WriteData/ReadData to DataWriter/DataReader ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1064))
* find something from STL instead TwoBitMask ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1101))
* move NetProperty to more proper place ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1319))
* improve YAML supporting to config file ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ConfigFile.cpp#L38))
* handle apply ([1](https://github.com/cvet/fonline/blob/master/Source/Common/FileSystem.cpp#L597))
* script ([1](https://github.com/cvet/fonline/blob/master/Source/Common/GenericUtils.cpp#L306))
* restore supporting of the map old text format ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.cpp#L41))
* pass errors vector to MapLoaderException ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.cpp#L146))
* remove mapper specific IsSelected from MapTile ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.h#L52))
* Check can get ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Properties.cpp#L1026))
* rework FONLINE_ ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Properties.cpp#L2224), [2](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L506), [3](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L977), [4](https://github.com/cvet/fonline/blob/master/Source/Common/Script.h#L146), [5](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L59), [6](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.cpp#L40), [7](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.h#L76), [8](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptPragmas.cpp#L41), [9](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptPragmas.h#L89))
* remove ProtoManager::ClearProtos ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ProtoManager.h#L44))
* settings const StrMap& config = MainConfig->GetApp(""); ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L1217))
* register new type with automating string - number convertation, exclude GetStrHash ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L70))
* const ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L246), [2](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L248))
* (Unnamed todo) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L113), [2](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L120), [3](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L127), [4](https://github.com/cvet/fonline/blob/master/Source/Editor/ModelBaker.cpp#L316), [5](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L495), [6](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L503), [7](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L513))
* Take Invoker from func ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.cpp#L367))
* remove VAR_SETTING ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.h#L38))
* hash ([1](https://github.com/cvet/fonline/blob/master/Source/Common/StringUtils.cpp#L523))
* restore stack trace dumping in file ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Testing.cpp#L672))
* Handle exceptions ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Testing.cpp#L683))
* !!! = entity->GetChildren(); ([1](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L1770))
* remove static SlotEnabled and SlotDataSendEnabled ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Critter.cpp#L48))
* IsPlaneNoTalk ([1](https://github.com/cvet/fonline/blob/master/Source/Server/CritterManager.cpp#L426), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4445))
* Restore mongodb (linux segfault and linking errors) ([1](https://github.com/cvet/fonline/blob/master/Source/Server/DataBase.cpp#L45))
* Check item name ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Dialogs.cpp#L474))
* encapsulate Location data ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Location.h#L81))
* HexFlags ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L590), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L700))
* if path finding not be reworked than migrate magic number to scripts ([1](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L1060))
* check group ([1](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L1644))
* settings ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L186), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2003))
* Restore hashes ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L1916))
* Clients logging may be not thread safe ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2145))
* Disable look distance caching ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2542))
* remove from game ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2937), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3137))
* Control max size explicitly, add option to property registration ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3568))
* Disable send changing field by client to this client ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3656))
* Clean up server 0 and -1 item ids ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4829))
* Add container properties changing notifications ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4859))
* Make BlockLines changable in runtime ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4938))
* need??? ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L5645))
* synchronize ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.h#L204))
* Make bundles for Mac and maybe iOS ([1](https://github.com/cvet/fonline/blob/master/CMakeLists.txt#L864))
  
## Repository structure

* *BuildScripts* - scripts for automatical build in command line or any ci/cd system
* *Resources* - resources for build applications but not related to code
* *SdkPlaceholder* - all this stuff merged with build output in resulted sdk zip
* *Source* - fonline engine specific code
* *ThirdParty* - external dependencies of engine, included to repository

## Clone / Build / Setup

You can build project by sh/bat script or directly use [CMake](https://cmake.org) generator.  
In any way first you must install CMake version equal or higher then 3.6.3.  
Information about build scripts you can find at BuildScripts/README.md.  
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
