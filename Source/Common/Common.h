//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Todo: rework all commented code during refactoring
// Todo: make entities positioning free in space, without hard-linking to hex
// Todo: add third 'up' coordinate to positioning that allow create multidimensional maps
// Todo: use Common.h as precompiled header
// Todo: use smart pointers instead raw
// Todo: fix all PVS Studio warnings
// Todo: SHA replace to openssl SHA
// Todo: add #undef for every local #define
// Todo: improve valgrind
// Todo: add behaviour for SDL_WINDOW_ALWAYS_ON_TOP
// Todo: move defines to const and enums
// Todo: don't use rtti/typeid and remove from compilation options?
// Todo: wrap fonline code to namespace?
// Todo: fix build warnings for all platforms
// Todo: enable threating warnings as errors
// Todo: id and hash to 8 byte integer
// Todo: research about std::string_view
// Todo: research about std::filesystem
// Todo: compile with -fpedantic
// Todo: c-style arrays to std::array
// Todo: use more STL (for ... -> auto p = find(begin(v), end(v), val); find_if, begin, end...)
// Todo: use par (for_each(par, v, [](int x))
// Todo: improve some single standard to initialize objects ({} or ())
// Todo: add constness as much as nessesary
// Todo: iterator -> const_iterator, auto -> const auto
// Todo: use using instead of typedef
// Todo: rework unscoped enums to scoped enums
// Todo: use more noexcept
// Todo: use more constexpr
// Todo: improve BitReader/BitWriter to better network/disk space utilization
// Todo: organize class members as public, protected, private; methods, fields
// Todo: prefer this construction if(auto i = do(); i < 0) i... else i...
// Todo: improve std::to_string or fmt::format to string conversions
// Todo: casts between int types via NumericCast<to>()
// Todo: minimize platform specific API (ifdef FO_os, WinApi_Include.h...)
// Todo: clang debug builds with sanitiziers
// Todo: time ticks to uint64
// Todo: improve custom exceptions for every subsustem
// Todo: improve particle system based on SPARK engine
// Todo: research about Steam integration
// Todo: speed up content loading from server
// Todo: temporary entities, disable writing to data base

#pragma once

#ifndef PRECOMPILED_HEADER_GUARD
#define PRECOMPILED_HEADER_GUARD

// Operating system (passed outside)
// Todo: detect OS automatically not from passed constant from build system
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB + FO_PS4 == 0
#error "Operating system not specified"
#endif
#if FO_WINDOWS + FO_LINUX + FO_MAC + FO_ANDROID + FO_IOS + FO_WEB + FO_PS4 != 1
#error "Multiple operating systems not allowed"
#endif

// Rendering
#if !(defined(FO_WINDOWS) && defined(WINRT))
#define FO_HAVE_OPENGL
#if defined(FO_IOS) || defined(FO_ANDROID) || defined(FO_WEB)
#define FO_OPENGL_ES
#endif
#endif
#if defined(FO_WINDOWS)
#define FO_HAVE_D3D
#endif
#if defined(FO_IOS)
// #define FO_HAVE_METAL
#endif
#if defined(FO_PS4)
#define FO_HAVE_GNM
#endif

// Standard API
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// OS specific API
#if defined(FO_MAC) || defined(FO_IOS)
#include <TargetConditionals.h>
#endif
#if defined(FO_WEB)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// String formatting lib
#include "fmt/format.h"

// Base types
// Todo: rename uchar to uint8 and use uint8_t as alias
// Todo: rename ushort to uint16 and use uint16_t as alias
// Todo: rename uint to uint32 and use uint32_t as alias
// Todo: rename char to int8 and use int8_t as alias
// Todo: split meanings if int8 and char in code
// Todo: rename short to int16 and use int16_t as alias
// Todo: rename int to int32 and use int32_t as alias
// Todo: move from 32 bit hashes to 64 bit
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using uint64 = uint64_t;
using int64 = int64_t;
using hash = uint;
using max_t = uint;

// Check the sizes of base types
static_assert(sizeof(char) == 1);
static_assert(sizeof(short) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(int64) == 8);
static_assert(sizeof(uchar) == 1);
static_assert(sizeof(ushort) == 2);
static_assert(sizeof(uint) == 4);
static_assert(sizeof(uint64) == 8);
static_assert(sizeof(bool) == 1);
static_assert(sizeof(void*) >= 4);
static_assert(sizeof(void*) == sizeof(size_t));

// Bind to global scope frequently used types
using std::array;
using std::deque;
using std::future;
using std::initializer_list;
using std::istringstream;
using std::list;
using std::map;
using std::multimap;
using std::optional;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::tuple;
using std::type_index;
using std::type_info;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::variant;
using std::vector;

// Math types
// Todo: replace depedency from assimp types (matrix/vector/quaternion/color)
#include "assimp/types.h"

using Vector = aiVector3D;
using VectorVec = vector<Vector>;
using Matrix = aiMatrix4x4;
using MatrixVec = vector<Matrix>;
using Quaternion = aiQuaternion;
using QuaternionVec = vector<Quaternion>;
using Color = aiColor4D;

// Todo: remove map/vector/set/pair bindings
using StrUCharMap = map<string, uchar>;
using UCharStrMap = map<uchar, string>;
using StrMap = map<string, string>;
using UIntStrMap = map<uint, string>;
using StrUShortMap = map<string, ushort>;
using StrUIntMap = map<string, uint>;
using StrInt64Map = map<string, int64>;
using StrPtrMap = map<string, void*>;
using UShortStrMap = map<ushort, string>;
using StrUIntMap = map<string, uint>;
using UIntMap = map<uint, uint>;
using IntMap = map<int, int>;
using IntFloatMap = map<int, float>;
using IntPtrMap = map<int, void*>;
using UIntFloatMap = map<uint, float>;
using UShortUIntMap = map<ushort, uint>;
using SizeTStrMap = map<size_t, string>;
using UIntIntMap = map<uint, int>;
using HashIntMap = map<hash, int>;
using HashUIntMap = map<hash, uint>;
using IntStrMap = map<int, string>;
using StrIntMap = map<string, int>;

using PtrVec = vector<void*>;
using IntVec = vector<int>;
using UCharVec = vector<uchar>;
using UCharVecVec = vector<UCharVec>;
using ShortVec = vector<short>;
using UShortVec = vector<ushort>;
using UIntVec = vector<uint>;
using UIntVecVec = vector<UIntVec>;
using CharVec = vector<char>;
using StrVec = vector<string>;
using PCharVec = vector<char*>;
using PUCharVec = vector<uchar*>;
using FloatVec = vector<float>;
using UInt64Vec = vector<uint64>;
using BoolVec = vector<bool>;
using SizeVec = vector<size_t>;
using HashVec = vector<hash>;
using HashVecVec = vector<HashVec>;
using MaxTVec = vector<max_t>;
using PStrMapVec = vector<StrMap*>;

using StrSet = set<string>;
using UCharSet = set<uchar>;
using UShortSet = set<ushort>;
using UIntSet = set<uint>;
using IntSet = set<int>;
using HashSet = set<hash>;

using IntPair = pair<int, int>;
using UShortPair = pair<ushort, ushort>;
using UIntPair = pair<uint, uint>;
using CharPair = pair<char, char>;
using PCharPair = pair<char*, char*>;
using UCharPair = pair<uchar, uchar>;

using UShortPairVec = vector<UShortPair>;
using IntPairVec = vector<IntPair>;
using UIntPairVec = vector<UIntPair>;
using PCharPairVec = vector<PCharPair>;
using UCharPairVec = vector<UCharPair>;

using UIntUIntPairMap = map<uint, UIntPair>;
using UIntIntPairVecMap = map<uint, IntPairVec>;
using UIntHashVecMap = map<uint, HashVec>;

// Generic engine exception
// Todo: rename GenericException to GenericException
class GenericException : public std::exception
{
public:
    template<typename... Args>
    GenericException(const char* message, Args... args) :
        exceptionParams {fmt::format("{}", std::forward<Args>(args))...}
    {
        exceptionMessage = "Exception: ";
        exceptionMessage.append(message);
        if (!exceptionParams.empty())
        {
            exceptionMessage.append("\n  Context args:");
            for (auto& param : exceptionParams)
                exceptionMessage.append("\n  - ").append(param);
        }
        exceptionMessage.append("\n  Traceback:");
        exceptionMessage.append("\n  - (todo)");
        exceptionMessage.append("\n  - (todo)");
        exceptionMessage.append("\n  - (todo)");
    }
    template<typename... Args>
    GenericException(const string& message, Args... args) :
        GenericException(message.c_str(), std::forward<Args>(args)...)
    {
    }
    ~GenericException() noexcept = default;
    const char* what() const noexcept override { return exceptionMessage.c_str(); }

private:
    string exceptionMessage {};
    // Todo: auto expand exception parameters to readable state
    vector<string> exceptionParams {};
};

#define DECLARE_EXCEPTION(exception_name) \
    class exception_name : public GenericException \
    { \
    public: \
        template<typename... Args> \
        exception_name(Args... args) : GenericException(std::forward<Args>(args)...) \
        { \
        } \
    }
#define BEGIN_ROOT_EXCEPTION_BLOCK() \
    try \
    {
#define END_ROOT_EXCEPTION_BLOCK() \
    } \
    catch (...) { throw; }
#define BEGIN_EXCEPTION_BLOCK() \
    try \
    {
#define END_EXCEPTION_BLOCK() \
    } \
    catch (GenericException & ex) { throw; }
#define CATCH_EXCEPTION(ex_type) \
    } \
    catch (ex_type & ex) \
    {
#define RUNTIME_ASSERT(expr) \
    if (!(expr)) \
    throw AssertationException(#expr, __FILE__, __LINE__)
#define RUNTIME_ASSERT_STR(expr, str) \
    if (!(expr)) \
    throw AssertationException(str, __FILE__, __LINE__)

DECLARE_EXCEPTION(AssertationException);
DECLARE_EXCEPTION(UnreachablePlaceException);

// Non copyable (but movable) class decorator
class NonCopyable // Todo: use NonCopyable as default class specifier
{
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

// Non movable (and copyable) class decorator
class NonMovable
{
public:
    NonMovable() = default;
    NonMovable(const NonMovable&) = delete;
    NonMovable& operator=(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

// Static class decorator
class StaticClass // Todo: set StaticClass class specifier to all static classes
{
public:
    StaticClass() = delete;
    StaticClass(const StaticClass&) = delete;
    StaticClass& operator=(const StaticClass&) = delete;
    StaticClass(StaticClass&&) = delete;
    StaticClass& operator=(StaticClass&&) = delete;
};

// Event system
class EventUnsubscriberCallback : public NonCopyable
{
    template<typename...>
    friend class EventObserver;
    friend class EventUnsubscriber;

private:
    using Callback = std::function<void()>;
    EventUnsubscriberCallback(Callback cb) : unsubscribeCallback {cb} {}
    Callback unsubscribeCallback {};
};

class EventUnsubscriber : public NonCopyable
{
    template<typename...>
    friend class EventObserver;

public:
    EventUnsubscriber() = default;
    EventUnsubscriber(EventUnsubscriber&&) = default;
    ~EventUnsubscriber() { Unsubscribe(); }
    EventUnsubscriber& operator+=(EventUnsubscriberCallback&& cb)
    {
        unsubscribeCallbacks.push_back(std::move(cb));
        return *this;
    }
    void Unsubscribe()
    {
        for (auto& cb : unsubscribeCallbacks)
            cb.unsubscribeCallback();
        unsubscribeCallbacks.clear();
    }

private:
    using Callback = std::function<void()>;
    EventUnsubscriber(EventUnsubscriberCallback cb) { unsubscribeCallbacks.push_back(std::move(cb)); }
    vector<EventUnsubscriberCallback> unsubscribeCallbacks {};
};

template<typename... Args>
class EventObserver : public NonCopyable, public NonMovable
{
    template<typename...>
    friend class EventDispatcher;

public:
    using Callback = std::function<void(Args...)>;

    EventObserver() = default;
    EventObserver(EventObserver&&) = default;
    ~EventObserver() { ThrowException(); }
    [[nodiscard]] EventUnsubscriberCallback operator+=(Callback cb)
    {
        auto it = subscriberCallbacks.insert(subscriberCallbacks.end(), cb);
        return EventUnsubscriberCallback([this, it]() { subscriberCallbacks.erase(it); });
    }

private:
    void ThrowException()
    {
#if 0
        if (!std::uncaught_exceptions())
            throw GenericException("Some of subscriber still alive", subscriberCallbacks.size());
#endif
    }

    list<Callback> subscriberCallbacks {};
};

template<typename... Args>
class EventDispatcher : public NonCopyable
{
public:
    using ObserverType = EventObserver<Args...>;
    EventDispatcher(ObserverType& obs) : observer {obs} {}
    EventDispatcher& operator()(Args... args)
    {
        // Todo: recursion guard for EventDispatcher
        if (!observer.subscriberCallbacks.empty())
            for (auto& cb : observer.subscriberCallbacks)
                cb(std::forward<Args>(args)...);
        return *this;
    }

private:
    ObserverType& observer;
};

// Raw pointer observation
// Todo: improve ptr<> system for leng term pointer observing
class Pointable : public NonMovable
{
    template<typename>
    friend class ptr;

public:
    virtual ~Pointable()
    {
        if (ptrCounter)
            ThrowException();
    }

private:
    void ThrowException()
    {
#if 0
        if (!std::uncaught_exceptions())
            throw GenericException("Some of pointer still alive", ptrCounter.load());
#endif
    }

    std::atomic_int ptrCounter {};
};

template<typename T>
class ptr
{
    static_assert(std::is_base_of<Pointable, T>::value, "T must inherit from Pointable");
    using type = ptr<T>;

public:
    ptr() = default;
    ptr(T* p) : value {p}
    {
        if (value)
            value->ptrCounter++;
    }
    ptr(const type& other)
    {
        value = other.value;
        value->ptrCounter++;
    }
    ptr& operator=(const type& other)
    {
        if (value)
            value->ptrCounter--;
        value = other.value;
        if (value)
            value->ptrCounter++;
    }
    ptr(type&& other)
    {
        value = other.value;
        other.value = nullptr;
    }
    ptr&& operator=(type&& other)
    {
        if (value)
            value->ptrCounter--;
        value = other.value;
        other.value = nullptr;
    }
    ~ptr()
    {
        if (value)
            value->ptrCounter--;
    }
    T* operator->() { return value; }
    T& operator*() { return *value; }
    operator bool() { return !!value; }
    bool operator==(const T* other) { return value == other; }
    bool operator==(const type& other) { return value == other.value; }
    bool operator!=(const T* other) { return value != other; }
    bool operator!=(const type& other) { return value != other.value; }
    // T* operator&() { return value; }
    // T* get() { return value; }

private:
    T* value {};
};

// C-strings literal helpers
// Todo: add _hash c-string literal helper
size_t constexpr operator"" _len(const char* str, size_t size)
{
    return size;
}

// Undef some global symbols from implicitly included WinRT API
#undef CopyFile
#undef DeleteFile
#undef PlaySound
#undef MessageBox
#undef GetObject
#undef LoadImage
#undef FindFirstFile
#undef FindNextFile
#undef GetClassName
#undef MessageBox
#undef Yield
#undef min
#undef max

// Generic helpers
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define LINE_STR STRINGIFY(__LINE__)
#define SCOPE_LOCK(m) std::lock_guard<std::mutex> _scope_lock(m) // Non-unique name to allow only one lock per scope
#define OFFSETOF(s, m) ((int)(size_t)(&reinterpret_cast<s*>(100000)->m) - 100000)
#define UNUSED_VARIABLE(x) (void)(x)
#define memzero(ptr, size) memset(ptr, 0, size)
#define UNIQUE_FUNCTION_NAME(name, ...) UNIQUE_FUNCTION_NAME2(MERGE_ARGS(name, __COUNTER__), __VA_ARGS__)
#define UNIQUE_FUNCTION_NAME2(name, ...) name(__VA_ARGS__)
#define MERGE_ARGS(a, b) MERGE_ARGS2(a, b)
#define MERGE_ARGS2(a, b) a##b
#define COLOR_RGBA(a, r, g, b) ((uint)((((a)&0xFF) << 24) | (((r)&0xFF) << 16) | (((g)&0xFF) << 8) | ((b)&0xFF)))
#define COLOR_RGB(r, g, b) COLOR_RGBA(0xFF, r, g, b)
#define COLOR_SWAP_RB(c) (((c)&0xFF00FF00) | (((c)&0x00FF0000) >> 16) | (((c)&0x000000FF) << 16))

// Todo: remove SAFEREL/SAFEDEL/SAFEDELA macro
#define SAFEREL(x) \
    { \
        if (x) \
            (x)->Release(); \
        (x) = nullptr; \
    }
#define SAFEDEL(x) \
    { \
        if (x) \
            delete (x); \
        (x) = nullptr; \
    }
#define SAFEDELA(x) \
    { \
        if (x) \
            delete[](x); \
        (x) = nullptr; \
    }

#define MAX(a, b) (((a) > (b)) ? (a) : (b)) // Todo: move MIN/MAX to std::min/std::max
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define PACKUINT64(u32hi, u32lo) (((uint64)u32hi << 32) | ((uint64)u32lo))
#define MAKEUINT(ch0, ch1, ch2, ch3) \
    ((uint)(uchar)(ch0) | ((uint)(uchar)(ch1) << 8) | ((uint)(uchar)(ch2) << 16) | ((uint)(uchar)(ch3) << 24))

// Floats
#define PI_VALUE (3.14159265f)
#define PI_FLOAT (3.14159265f)
#define SQRT3T2_FLOAT (3.4641016151f)
#define SQRT3_FLOAT (1.732050807568877f)
#define BIAS_FLOAT (0.02f)
#define RAD2DEG (57.29577951f)
#define RAD(deg) ((deg)*3.141592654f / 180.0f)
#define MAX_INT (0x7FFFFFFF)

// Bits
#define BIN__N(x) (x) | x >> 3 | x >> 6 | x >> 9
#define BIN__B(x) (x) & 0xf | (x) >> 12 & 0xf0
#define BIN8(v) (BIN__B(BIN__N(0x##v)))
#define BIN16(bin16, bin8) ((BIN8(bin16) << 8) | (BIN8(bin8)))
#define BIN32(bin32, bin24, bin16, bin8) ((BIN8(bin32) << 24) | (BIN8(bin24) << 16) | (BIN8(bin16) << 8) | (BIN8(bin8)))

// Flags
#define FLAG(x, y) (((x) & (y)) != 0)
#define FLAGS(x, y) (((x) & (y)) == y)
#define SETFLAG(x, y) ((x) = (x) | (y))
#define UNSETFLAG(x, y) ((x) = ((x) | (y)) ^ (y))

// Generic constants
// Todo: eliminate as much defines as possible
// Todo: convert all defines to constants and enums
#define CONFIG_NAME "FOnline.cfg"
#define WORLD_START_TIME "07:00 30:10:2246 x00"
#define TEMP_BUF_SIZE (8192)
#define UTF8_BUF_SIZE(count) ((count)*4)
#define MAX_FOPATH UTF8_BUF_SIZE(2048)
#define MAX_HOLO_INFO (250)
#define IS_DIR_CORNER(dir) (((dir)&1) != 0) // 1, 3, 5, 7
#define AGGRESSOR_TICK (60000)
#define MAX_ENEMY_STACK (30)
#define CRITTER_INV_VOLUME (1000)
#define CLIENT_MAP_FORMAT_VER (10)
#define MAX_ANSWERS (100)
#define PROCESS_TALK_TICK (1000)
#define TURN_BASED_TIMEOUT (1000)
#define FADING_PERIOD (1000)
#define RESPOWN_TIME_PLAYER (3)
#define RESPOWN_TIME_NPC (120)
#define RESPOWN_TIME_INFINITY (4 * 24 * 60 * 60000)
#define AP_DIVIDER (100)
#define MAX_ADDED_NOGROUP_ITEMS (30)
#define LAYERS3D_COUNT (30)
#define DEFAULT_DRAW_SIZE (128)
#define MIN_ZOOM (0.1f)
#define MAX_ZOOM (20.0f)

// Id helpers
#define MAKE_CLIENT_ID(name) ((1 << 31) | _str(name).toHash())
#define IS_CLIENT_ID(id) (((id) >> 31) != 0)
#define DLGID_MASK (0xFFFFC000)
#define DLG_STR_ID(dlg_id, idx) (((dlg_id)&DLGID_MASK) | ((idx) & ~DLGID_MASK))
#define LOCPID_MASK (0xFFFFF000)
#define LOC_STR_ID(loc_pid, idx) (((loc_pid)&LOCPID_MASK) | ((idx) & ~LOCPID_MASK))
#define ITEMPID_MASK (0xFFFFFFF0)
#define ITEM_STR_ID(item_pid, idx) (((item_pid)&ITEMPID_MASK) | ((idx) & ~ITEMPID_MASK))
#define CRPID_MASK (0xFFFFFFF0)
#define CR_STR_ID(cr_pid, idx) (((cr_pid)&CRPID_MASK) | ((idx) & ~CRPID_MASK))

// Critter find types
#define FIND_LIFE (0x01)
#define FIND_KO (0x02)
#define FIND_DEAD (0x04)
#define FIND_ONLY_PLAYERS (0x10)
#define FIND_ONLY_NPC (0x20)
#define FIND_ALL (0x0F)

// Ping
#define PING_PING (0)
#define PING_WAIT (1)
#define PING_CLIENT (2)

// Say types
#define SAY_NORM (1)
#define SAY_NORM_ON_HEAD (2)
#define SAY_SHOUT (3)
#define SAY_SHOUT_ON_HEAD (4)
#define SAY_EMOTE (5)
#define SAY_EMOTE_ON_HEAD (6)
#define SAY_WHISP (7)
#define SAY_WHISP_ON_HEAD (8)
#define SAY_SOCIAL (9)
#define SAY_RADIO (10)
#define SAY_NETMSG (11)
#define SAY_DIALOG (12)
#define SAY_APPEND (13)
#define SAY_FLASH_WINDOW (41)

// Target types
#define TARGET_SELF (0)
#define TARGET_SELF_ITEM (1)
#define TARGET_CRITTER (2)
#define TARGET_ITEM (3)

// Global map
#define GM__MAXZONEX (100)
#define GM__MAXZONEY (100)
#define GM_ZONES_FOG_SIZE (((GM__MAXZONEX / 4) + ((GM__MAXZONEX % 4) ? 1 : 0)) * GM__MAXZONEY)
#define GM_FOG_FULL (0)
#define GM_FOG_HALF (1)
#define GM_FOG_HALF_EX (2)
#define GM_FOG_NONE (3)
#define GM_ANSWER_WAIT_TIME (20000)
#define GM_LIGHT_TIME (5000)
#define GM_ENTRANCES_SEND_TIME (60000)
#define GM_TRACE_TIME (1000)

// GM Info
#define GM_INFO_LOCATIONS (0x01)
#define GM_INFO_CRITTERS (0x02)
#define GM_INFO_ZONES_FOG (0x08)
#define GM_INFO_ALL (0x0F)
#define GM_INFO_FOG (0x10)
#define GM_INFO_LOCATION (0x20)

// Proto map hex flags
#define FH_BLOCK BIN8(00000001)
#define FH_NOTRAKE BIN8(00000010)
#define FH_STATIC_TRIGGER BIN8(00100000)

// Map instance hex flags
#define FH_CRITTER BIN8(00000001)
#define FH_DEAD_CRITTER BIN8(00000010)
#define FH_DOOR BIN8(00001000)
#define FH_BLOCK_ITEM BIN8(00010000)
#define FH_NRAKE_ITEM BIN8(00100000)
#define FH_TRIGGER BIN8(01000000)
#define FH_GAG_ITEM BIN8(10000000)

// Both proto map and map instance flags
#define FH_NOWAY BIN16(00010001, 00000001)
#define FH_NOSHOOT BIN16(00100000, 00000010)

// Coordinates
#define MAXHEX_DEF (200)
#define MAXHEX_MIN (10)
#define MAXHEX_MAX (4000)

// Client parameters
#define MAX_NAME (30)
#define MIN_NAME (1)
#define MAX_CHAT_MESSAGE (100)
#define MAX_SCENERY (5000)
#define MAX_DIALOG_TEXT (MAX_FOTEXT)
#define MAX_DLG_LEN_IN_BYTES (64 * 1024)
#define MAX_DLG_LEXEMS_TEXT (1000)
#define MAX_BUF_LEN (4096)
#define PASS_HASH_SIZE (32)
#define FILE_UPDATE_PORTION (16384)

// Answer
#define ANSWER_BEGIN (0xF0)
#define ANSWER_END (0xF1)
#define ANSWER_BARTER (0xF2)

// Crit conditions
#define COND_LIFE (1)
#define COND_KNOCKOUT (2)
#define COND_DEAD (3)

// Run-time critters flags
#define FCRIT_PLAYER (0x00010000)
#define FCRIT_NPC (0x00020000)
#define FCRIT_DISCONNECT (0x00080000)
#define FCRIT_CHOSEN (0x00100000)

// Show screen modes
// Ouput: it is 'uint param' in Critter::ShowScreen.
// Input: I - integer value 'uint answerI', S - string value 'string& answerS' in 'answer_' function.
#define SHOW_SCREEN_CLOSE (0) // Close top window.
#define SHOW_SCREEN_TIMER (1) // Timer box. Output: picture index in INVEN.LST. Input I: time in game minutes (1..599).
#define SHOW_SCREEN_DIALOGBOX (2) // Dialog box. Output: buttons - 0..20. Input I: Choosed button - 0..19.
#define SHOW_SCREEN_SKILLBOX (3) // Skill box. Input I: selected skill.
#define SHOW_SCREEN_BAG (4) // Bag box. Input I: id of selected item.
#define SHOW_SCREEN_SAY (5) // Say box. Output: all symbols - 0 or only numbers - any number. Input S: typed string.
#define SHOW_ELEVATOR (6) // Elevator. Output: look ELEVATOR_* macro. Input I: Choosed level button.
#define SHOW_SCREEN_INVENTORY (7) // Inventory.
#define SHOW_SCREEN_CHARACTER (8) // Character.
#define SHOW_SCREEN_FIXBOY (9) // Fix-boy.
#define SHOW_SCREEN_PIPBOY (10) // Pip-boy.
#define SHOW_SCREEN_MINIMAP (11) // Mini-map.

// Special send params
#define OTHER_BREAK_TIME (0)
#define OTHER_WAIT_TIME (1)
#define OTHER_FLAGS (2)
#define OTHER_CLEAR_MAP (6)
#define OTHER_TELEPORT (7)

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//  flags actionExt item
#define ACTION_MOVE_ITEM (2) // l s from slot +
#define ACTION_MOVE_ITEM_SWAP (3) // l s from slot +
#define ACTION_DROP_ITEM (5) // l s from slot +
#define ACTION_KNOCKOUT (16) // s 0 - knockout anim2begin
#define ACTION_STANDUP (17) // s 0 - knockout anim2end
#define ACTION_FIDGET (18) // l
#define ACTION_DEAD (19) // s dead type anim2 (see Anim2 in _animation.fos)
#define ACTION_CONNECT (20)
#define ACTION_DISCONNECT (21)
#define ACTION_RESPAWN (22) // s
#define ACTION_REFRESH (23) // s

// Look checks
#define LOOK_CHECK_DIR (0x01)
#define LOOK_CHECK_SNEAK_DIR (0x02)
#define LOOK_CHECK_TRACE (0x08)
#define LOOK_CHECK_SCRIPT (0x10)
#define LOOK_CHECK_ITEM_SCRIPT (0x20)

// SendMessage types
#define MESSAGE_TO_VISIBLE_ME (0)
#define MESSAGE_TO_IAM_VISIBLE (1)
#define MESSAGE_TO_ALL_ON_MAP (2)

// Anims
#define ANIM1_UNARMED (1)
#define ANIM2_IDLE (1)
#define ANIM2_WALK (3)
#define ANIM2_LIMP (4)
#define ANIM2_RUN (5)
#define ANIM2_PANIC_RUN (6)
#define ANIM2_SNEAK_WALK (7)
#define ANIM2_SNEAK_RUN (8)
#define ANIM2_IDLE_PRONE_FRONT (86)
#define ANIM2_IDLE_PRONE_BACK (87)
#define ANIM2_DEAD_FRONT (102)
#define ANIM2_DEAD_BACK (103)

// Move params
// 6 next steps (each 5 bit) + stop bit + run bit
// Step bits: 012 - dir, 3 - allow, 4 - disallow
#define MOVE_PARAM_STEP_COUNT (6)
#define MOVE_PARAM_STEP_BITS (5)
#define MOVE_PARAM_STEP_DIR (0x7)
#define MOVE_PARAM_STEP_ALLOW (0x8)
#define MOVE_PARAM_STEP_DISALLOW (0x10)
#define MOVE_PARAM_RUN (0x80000000)

// Corner type
#define CORNER_NORTH_SOUTH (0)
#define CORNER_WEST (1)
#define CORNER_EAST (2)
#define CORNER_SOUTH (3)
#define CORNER_NORTH (4)
#define CORNER_EAST_WEST (5)

// Items accessory
#define ITEM_ACCESSORY_NONE (0)
#define ITEM_ACCESSORY_CRITTER (1)
#define ITEM_ACCESSORY_HEX (2)
#define ITEM_ACCESSORY_CONTAINER (3)

// Uses
#define USE_PRIMARY (0)
#define USE_SECONDARY (1)
#define USE_THIRD (2)
#define USE_RELOAD (3)
#define USE_USE (4)
#define MAX_USES (3)
#define USE_NONE (15)
#define MAKE_ITEM_MODE(use, aim) ((((aim) << 4) | ((use)&0xF)) & 0xFF)

// Radio
#define RADIO_DISABLE_SEND (0x01)
#define RADIO_DISABLE_RECV (0x02)
#define RADIO_BROADCAST_WORLD (0)
#define RADIO_BROADCAST_MAP (20)
#define RADIO_BROADCAST_LOCATION (40)
#define RADIO_BROADCAST_ZONE(x) (100 + CLAMP(x, 1, 100)) // 1..100
#define RADIO_BROADCAST_FORCE_ALL (250)

// Light
#define LIGHT_DISABLE_DIR(dir) (1 << CLAMP(dir, 0, 5))
#define LIGHT_GLOBAL (0x40)
#define LIGHT_INVERSE (0x80)

// Access
#define ACCESS_CLIENT (0)
#define ACCESS_TESTER (1)
#define ACCESS_MODER (2)
#define ACCESS_ADMIN (3)
#define ACCESS_DEFAULT ACCESS_CLIENT

// Commands
#define CMD_EXIT (1)
#define CMD_MYINFO (2)
#define CMD_GAMEINFO (3)
#define CMD_CRITID (4)
#define CMD_MOVECRIT (5)
#define CMD_DISCONCRIT (7)
#define CMD_TOGLOBAL (8)
#define CMD_PROPERTY (10)
#define CMD_GETACCESS (11)
#define CMD_ADDITEM (12)
#define CMD_ADDITEM_SELF (14)
#define CMD_ADDNPC (15)
#define CMD_ADDLOCATION (16)
#define CMD_RUNSCRIPT (20)
#define CMD_REGENMAP (25)
#define CMD_SETTIME (32)
#define CMD_BAN (33)
#define CMD_DELETE_ACCOUNT (34)
#define CMD_CHANGE_PASSWORD (35)
#define CMD_LOG (37)
#define CMD_DEV_EXEC (38)
#define CMD_DEV_FUNC (39)
#define CMD_DEV_GVAR (40)

// Memory pool
template<int StructSize, int PoolSize>
class MemoryPool
{
private:
    vector<char*> allocatedData;

    void Grow()
    {
        allocatedData.reserve(allocatedData.size() + PoolSize);
        for (int i = 0; i < PoolSize; i++)
            allocatedData.push_back(new char[StructSize]);
    }

public:
    MemoryPool() { Grow(); }

    ~MemoryPool()
    {
        for (auto it = allocatedData.begin(); it != allocatedData.end(); ++it)
            delete[] * it;
        allocatedData.clear();
    }

    void* Get()
    {
        if (allocatedData.empty())
            Grow();
        void* result = allocatedData.back();
        allocatedData.pop_back();
        return result;
    }

    void Put(void* t) { allocatedData.push_back((char*)t); }
};

// Data serialization helpers
class DataReader
{
public:
    DataReader(const uchar* buf) : dataBuf {buf}, readPos {} {}
    DataReader(const UCharVec& buf) : dataBuf {!buf.empty() ? &buf[0] : nullptr}, readPos {} {}

    template<class T>
    inline T Read()
    {
        T data;
        memcpy(&data, &dataBuf[readPos], sizeof(T));
        readPos += sizeof(T);
        return data;
    }

    template<class T>
    inline const T* ReadPtr(size_t size)
    {
        readPos += size;
        return size ? &dataBuf[readPos - size] : nullptr;
    }

    template<class T>
    inline void ReadPtr(T* ptr, size_t size)
    {
        readPos += size;
        if (size)
            memcpy(ptr, &dataBuf[readPos - size], size);
    }

private:
    const uchar* dataBuf;
    size_t readPos;
};

class DataWriter
{
public:
    DataWriter(UCharVec& buf) : dataBuf {buf} {}

    template<class T>
    inline void Write(T data)
    {
        size_t cur = dataBuf.size();
        dataBuf.resize(cur + sizeof(data));
        memcpy(&dataBuf[cur], &data, sizeof(data));
    }

    template<class T>
    inline void WritePtr(T* data, size_t size)
    {
        if (!size)
            return;

        size_t cur = dataBuf.size();
        dataBuf.resize(cur + size);
        memcpy(&dataBuf[cur], data, size);
    }

private:
    UCharVec& dataBuf;
};

// Todo: move WriteData/ReadData to DataWriter/DataReader
template<class T>
inline void WriteData(UCharVec& vec, T data)
{
    size_t cur = vec.size();
    vec.resize(cur + sizeof(data));
    memcpy(&vec[cur], &data, sizeof(data));
}

template<class T>
inline void WriteDataArr(UCharVec& vec, T* data, uint size)
{
    if (size)
    {
        uint cur = (uint)vec.size();
        vec.resize(cur + size);
        memcpy(&vec[cur], data, size);
    }
}

template<class T>
inline T ReadData(UCharVec& vec, uint& pos)
{
    T data;
    memcpy(&data, &vec[pos], sizeof(T));
    pos += sizeof(T);
    return data;
}

template<class T>
inline T* ReadDataArr(UCharVec& vec, uint size, uint& pos)
{
    pos += size;
    return size ? &vec[pos - size] : nullptr;
}

// Two bit mask
// Todo: find something from STL instead TwoBitMask
class TwoBitMask
{
public:
    TwoBitMask() = default;

    TwoBitMask(uint width_2bit, uint height_2bit, uchar* ptr)
    {
        if (!width_2bit)
            width_2bit = 1;
        if (!height_2bit)
            height_2bit = 1;

        width = width_2bit;
        height = height_2bit;
        widthBytes = width / 4;

        if (width % 4)
            widthBytes++;

        if (ptr)
        {
            isAlloc = false;
            data = ptr;
        }
        else
        {
            isAlloc = true;
            data = new uchar[widthBytes * height];
            Fill(0);
        }
    }

    ~TwoBitMask()
    {
        if (isAlloc)
            delete[] data;
        data = nullptr;
    }

    void Set2Bit(uint x, uint y, int val)
    {
        if (x >= width || y >= height)
            return;

        uchar& b = data[y * widthBytes + x / 4];
        int bit = (x % 4 * 2);

        UNSETFLAG(b, 3 << bit);
        SETFLAG(b, (val & 3) << bit);
    }

    int Get2Bit(uint x, uint y)
    {
        if (x >= width || y >= height)
            return 0;

        return (data[y * widthBytes + x / 4] >> (x % 4 * 2)) & 3;
    }

    void Fill(int fill) { memset(data, fill, widthBytes * height); }
    uchar* GetData() { return data; }

private:
    bool isAlloc {};
    uchar* data {};
    uint width {};
    uint height {};
    uint widthBytes {};
};

// Flex rect
template<typename Ty>
struct FlexRect
{
    FlexRect() : L(0), T(0), R(0), B(0) {}
    template<typename Ty2>
    FlexRect(const FlexRect<Ty2>& fr) : L((Ty)fr.L), T((Ty)fr.T), R((Ty)fr.R), B((Ty)fr.B)
    {
    }
    FlexRect(Ty l, Ty t, Ty r, Ty b) : L(l), T(t), R(r), B(b) {}
    FlexRect(Ty l, Ty t, Ty r, Ty b, Ty ox, Ty oy) : L(l + ox), T(t + oy), R(r + ox), B(b + oy) {}
    FlexRect(const FlexRect& fr, Ty ox, Ty oy) : L(fr.L + ox), T(fr.T + oy), R(fr.R + ox), B(fr.B + oy) {}

    template<typename Ty2>
    FlexRect& operator=(const FlexRect<Ty2>& fr)
    {
        L = (Ty)fr.L;
        T = (Ty)fr.T;
        R = (Ty)fr.R;
        B = (Ty)fr.B;
        return *this;
    }

    void Clear()
    {
        L = 0;
        T = 0;
        R = 0;
        B = 0;
    }

    bool IsZero() const { return !L && !T && !R && !B; }
    Ty W() const { return R - L + 1; }
    Ty H() const { return B - T + 1; }
    Ty CX() const { return L + W() / 2; }
    Ty CY() const { return T + H() / 2; }

    Ty& operator[](int index)
    {
        switch (index)
        {
        case 0:
            return L;
        case 1:
            return T;
        case 2:
            return R;
        case 3:
            return B;
        default:
            break;
        }
        return L;
    }

    FlexRect& operator()(Ty l, Ty t, Ty r, Ty b)
    {
        L = l;
        T = t;
        R = r;
        B = b;
        return *this;
    }

    FlexRect& operator()(Ty ox, Ty oy)
    {
        L += ox;
        T += oy;
        R += ox;
        B += oy;
        return *this;
    }

    FlexRect<Ty> Interpolate(const FlexRect<Ty>& to, int procent)
    {
        FlexRect<Ty> result(L, T, R, B);
        result.L += (Ty)((int)(to.L - L) * procent / 100);
        result.T += (Ty)((int)(to.T - T) * procent / 100);
        result.R += (Ty)((int)(to.R - R) * procent / 100);
        result.B += (Ty)((int)(to.B - B) * procent / 100);
        return result;
    }

    Ty L {};
    Ty T {};
    Ty R {};
    Ty B {};
};
using Rect = FlexRect<int>;
using RectF = FlexRect<float>;
using IntRectVec = std::vector<Rect>;
using FltRectVec = std::vector<RectF>;

template<typename Ty>
struct FlexPoint
{
    FlexPoint() : X(0), Y(0) {}
    template<typename Ty2>
    FlexPoint(const FlexPoint<Ty2>& r) : X((Ty)r.X), Y((Ty)r.Y)
    {
    }
    FlexPoint(Ty x, Ty y) : X(x), Y(y) {}
    FlexPoint(const FlexPoint& fp, Ty ox, Ty oy) : X(fp.X + ox), Y(fp.Y + oy) {}

    template<typename Ty2>
    FlexPoint& operator=(const FlexPoint<Ty2>& fp)
    {
        X = (Ty)fp.X;
        Y = (Ty)fp.Y;
        return *this;
    }

    void Clear()
    {
        X = 0;
        Y = 0;
    }

    bool IsZero() { return !X && !Y; }

    Ty& operator[](int index)
    {
        switch (index)
        {
        case 0:
            return X;
        case 1:
            return Y;
        default:
            break;
        }
        return X;
    }

    FlexPoint& operator()(Ty x, Ty y)
    {
        X = x;
        Y = y;
        return *this;
    }

    Ty X {};
    Ty Y {};
};
using Point = FlexPoint<int>;
using PointF = FlexPoint<float>;

// Todo: move NetProperty to more proper place
class NetProperty
{
public:
    enum Type
    {
        None = 0,
        Global, // 0
        Critter, // 1 cr_id
        Chosen, // 0
        MapItem, // 1 item_id
        CritterItem, // 2 cr_id item_id
        ChosenItem, // 1 item_id
        Map, // 0
        Location, // 0
    };
};

extern bool Is3dExtensionSupported(const string& ext);

#endif // PRECOMPILED_HEADER_GUARD
