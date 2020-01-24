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

* use Common.h as precompiled header ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L36))
* review push_back -> emplace_back ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L37))
* use smart pointers instead raw ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L38))
* fix all PVS Studio warnings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L39))
* SHA replace to openssl SHA ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L40))
* add #undef for every local #define ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L41))
* need Linux 32-bit builds? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L42))
* improve valgrind ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L43))
* add behaviour for SDL_WINDOW_ALWAYS_ON_TOP ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L44))
* move defines to const and enums ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L45))
* don't use rtti and remove from compilation options ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L46))
* research mac cross-compile https://github.com/tpoechtrager/osxcross ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L47))
* research windows cross-compile https://arrayfire.com/cross-compile-to-windows-from-linux/ ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L48))
* wrap fonline code to namespace ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L49))
* fix LINK : warning LNK4044: unrecognized option '/INCREMENTAL:NO'; ignored ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L50))
* fix LINK : warning LNK4044: unrecognized option '/MANIFEST:NO'; ignored ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L51))
* fix build warnings for all platforms ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L52))
* sound and video preprocessing move to editor ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L53))
* research about engine version naming convention (maybe 2019.1/2/3/4?) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L54))
* id and hash to 8 byte integer ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L55))
* add to copyrigths for https://github.com/taka-no-me/android-cmake ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L56))
* llvm as main compiler? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L57))
* make all depedencies as git submodules? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L58))
* research about std::string_view ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L59))
* research about std::filesystem ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L60))
* compile with -fpedantic ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L61))
* c-style arrays to std::array ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L62))
* use more STL (for ... -> auto p = find(begin(v), end(v), val); find_if, begin, end...) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L63))
* use par (for_each(par, v, [](int x)) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L64))
* improve some single standard to initialize objects ({} or ()) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L65))
* iterator -> const_iterator, auto -> const auto ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L66))
* add constness as much as nessesary ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L67))
* use using instead of typedef ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L68))
* rework unscoped enums to scoped enums ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L69))
* use more noexcept ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L70))
* use more constexpr ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L71))
* improve BitReader/BitWriter to better network/disk space utilization ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L72))
* organize class members as public, protected, private; methods, fields ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L73))
* research c++20 modules ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L74))
* eliminate volatile, replace to atomic if needed ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L75))
* change Visual Studio toolset to clang-cl? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L76))
* prefer this construction if(auto i = do(); i < 0) i... else i... ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L77))
* improve std::to_string or fmt::format to string conversions ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L78))
* casts between int types via NumericCast<to>() ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L79))
* minimize platform specific API (ifdef FO_os, WinApi_Include.h...) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L80))
* build debug sanitiziers ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L81))
* time ticks to uint64 ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L82))
* improve custom exceptions for every subsustem ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L83))
* reasearch about std::quick_exit ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L84))
* detect OS automatically not from passed constant from build system ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L93))
* rename uchar to uint8 and use uint8_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L155))
* rename ushort to uint16 and use uint16_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L156))
* rename uint to uint32 and use uint32_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L157))
* rename char to int8 and use int8_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L158))
* split meanings if int8 and char in code ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L159))
* rename short to int16 and use int16_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L160))
* rename int to int32 and use int32_t as alias ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L161))
* move from 32 bit hashes to 64 bit ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L162))
* remove map/vector/set/pair bindings ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L206))
* auto expand exception parameters to readable state ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L289))
* use NonCopyable as default class specifier ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L304))
* set StaticClass class specifier to all static classes ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L326))
* recursion guard for EventDispatcher ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L412))
* improve ptr<> system for leng term pointer observing ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L424))
* add _hash c-string literal helper ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L505))
* remove SAFEREL/SAFEDEL/SAFEDELA macro ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L526))
* move MIN/MAX to std::min/std::max ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L546))
* eliminate as much defines as possible ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L577))
* convert all defines to constants and enums ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L578))
* move WriteData/ReadData to DataWriter/DataReader ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L977))
* find something from STL instead TwoBitMask ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1014))
* move NetProperty to more proper place ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Common.h#L1232))
* improve editor Undo/Redo ([1](https://github.com/cvet/fonline/blob/master/Source/Applications/EditorApp.cpp#L34))
* need attention! ([1](https://github.com/cvet/fonline/blob/master/Source/Applications/EditorApp.cpp#L215), [2](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L3907), [3](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L6099), [4](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L4311), [5](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L4432), [6](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L113), [7](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L120), [8](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L127), [9](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L192), [10](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L856), [11](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L1770), [12](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2277), [13](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2311), [14](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2333), [15](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2357), [16](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L2662), [17](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3318), [18](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3356), [19](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3365), [20](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3435), [21](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3449), [22](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3464), [23](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3507), [24](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3588), [25](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3601), [26](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3615), [27](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L3980), [28](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4025), [29](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4525), [30](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4811), [31](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4846), [32](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4923), [33](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.cpp#L4946), [34](https://github.com/cvet/fonline/blob/master/Source/Editor/ModelBaker.cpp#L316), [35](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L230), [36](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L5644))
* add interpolation for tracks more than two ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dAnimation.cpp#L420))
* add reverse playing of 3d animation ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dStuff.cpp#L2183))
* process default animations ([1](https://github.com/cvet/fonline/blob/master/Source/Client/3dStuff.cpp#L2397))
* synchronize effects showing (for example shot and kill) ([1](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp#L3375))
* migrate critter on head text moving in scripts ([1](https://github.com/cvet/fonline/blob/master/Source/Client/CritterView.cpp#L617))
* do same for 2d animations ([1](https://github.com/cvet/fonline/blob/master/Source/Client/CritterView.cpp#L974))
* optimize lighting rebuilding to skip unvisible lights ([1](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.cpp#L1878))
* move HexManager to MapView? ([1](https://github.com/cvet/fonline/blob/master/Source/Client/HexManager.h#L34))
* improve DirectX rendering ([1](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L34))
* maybe restrict fps at 60? ([1](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L35))
* improve client rendering brightness ([1](https://github.com/cvet/fonline/blob/master/Source/Client/SpriteManager.h#L62))
* store Cache.bin in player local dir for Windows users? ([1](https://github.com/cvet/fonline/blob/master/Source/Common/CacheStorage.h#L34))
* improve YAML supporting to config file ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ConfigFile.cpp#L38))
* handle apply file writing ([1](https://github.com/cvet/fonline/blob/master/Source/Common/FileSystem.cpp#L597))
* script handling in ConvertParamValue ([1](https://github.com/cvet/fonline/blob/master/Source/Common/GenericUtils.cpp#L306))
* server logs append not rewrite (with checking of size) ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Log.cpp#L34))
* add timestamps and process id and thread id to file logs ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Log.cpp#L35))
* delete \n appendix from WriteLog ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Log.cpp#L36))
* restore supporting of the map old text format ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.cpp#L41))
* pass errors vector to MapLoaderException ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.cpp#L146))
* remove mapper specific IsSelected from MapTile ([1](https://github.com/cvet/fonline/blob/master/Source/Common/MapLoader.h#L52))
* check can get in SetGetCallback ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Properties.cpp#L1026))
* rework FONLINE_ ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Properties.cpp#L2224), [2](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L506), [3](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L977), [4](https://github.com/cvet/fonline/blob/master/Source/Common/Script.h#L148), [5](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptFunctions_Include.h#L59), [6](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.cpp#L40), [7](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.h#L76), [8](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptPragmas.cpp#L41), [9](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptPragmas.h#L89))
* remove ProtoManager::ClearProtos ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ProtoManager.h#L44))
* fill settings to scripts ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Script.cpp#L1217))
* rework Script singleton ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Script.h#L34))
* register new type with automating string - number convertation, exclude GetStrHash ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L70))
* const Crit_GetCritters ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L246))
* const Npc_GetTalkedPlayers ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptBind_Include.h#L248))
* take Invoker from func ([1](https://github.com/cvet/fonline/blob/master/Source/Common/ScriptInvoker.cpp#L367))
* remove VAR_SETTING must stay only constant values ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Settings.h#L38))
* restore hash parsing ([1](https://github.com/cvet/fonline/blob/master/Source/Common/StringUtils.cpp#L523))
* restore stack trace dumping in file ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Testing.cpp#L672))
* improve global exceptions handlers for mobile os ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Testing.cpp#L683))
* review RUNTIME_ASSERT( ( ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Testing.h#L34))
* timers to std::chrono ([1](https://github.com/cvet/fonline/blob/master/Source/Common/Timer.h#L34))
* move WinApi to separate module because it's give too much garbage in global namespace ([1](https://github.com/cvet/fonline/blob/master/Source/Common/WinApi_Include.h#L34))
* finish with GLSL to SPIRV to GLSL/HLSL/MSL ([1](https://github.com/cvet/fonline/blob/master/Source/Editor/ImageBaker.cpp#L34))
* add standalone Mapper application ([1](https://github.com/cvet/fonline/blob/master/Source/Editor/Mapper.h#L34))
* admin panel network to Asio ([1](https://github.com/cvet/fonline/blob/master/Source/Server/AdminPanel.h#L34))
* remove static SlotEnabled and SlotDataSendEnabled ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Critter.cpp#L48))
* don't remeber but need check (IsPlaneNoTalk) ([1](https://github.com/cvet/fonline/blob/master/Source/Server/CritterManager.cpp#L426), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4445))
* restore mongodb (linux segfault and linking errors) ([1](https://github.com/cvet/fonline/blob/master/Source/Server/DataBase.cpp#L45))
* check item name on DR_ITEM ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Dialogs.cpp#L474))
* encapsulate Location data ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Location.h#L81))
* rework FOREACH_PROTO_ITEM_LINES ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L495), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L503), [3](https://github.com/cvet/fonline/blob/master/Source/Server/Map.cpp#L513))
* if path finding not be reworked than migrate magic number to scripts ([1](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L1060))
* check group ([1](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp#L1644))
* restore settings ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L186), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2003))
* restore hashes loading ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L1916))
* clients logging may be not thread safe ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2145))
* disable look distance caching ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2542))
* remove from game ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L2937), [2](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3137))
* control max size explicitly, add option to property registration ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3568))
* disable send changing field by client to this client ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L3656))
* add container properties changing notifications ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4858))
* make BlockLines changable in runtime ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp#L4937))
* synchronize LangPacks ([1](https://github.com/cvet/fonline/blob/master/Source/Server/Server.h#L204))
  
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
