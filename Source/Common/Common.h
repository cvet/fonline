//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: make entities positioning free in space, without hard-linking to hex
// Todo: add third 'up' coordinate to positioning that allow create multidimensional maps
// Todo: use smart pointers instead raw
// Todo: fix all PVS Studio warnings
// Todo: SHA replace to openssl SHA
// Todo: wrap fonline code to namespace
// Todo: hash_t 8 byte integer
// Todo: c-style arrays to std::array
// Todo: use more noexcept
// Todo: use more constexpr
// Todo: improve BitReader/BitWriter to better network/disk space utilization
// Todo: improve custom exceptions for every subsustem
// Todo: temporary entities, disable writing to data base
// Todo: move all return values from out refs to return values as tuple and nodiscard (and then use structuured binding)
// Todo: split meanings of int8/char and uint8/byte in code

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
// ReSharper disable CppInconsistentNaming

#pragma once

#ifndef FO_PRECOMPILED_HEADER_GUARD
#define FO_PRECOMPILED_HEADER_GUARD

// Essential headers
// In order of dependency
// clang-format off
#include "BasicCore.h"
#include "GlobalData.h"
#include "BaseLogging.h"
#include "StackTrace.h"
#include "SmartPointers.h"
#include "MemorySystem.h"
#include "Containers.h"
#include "StringUtils.h"
#include "DiskFileSystem.h"
#include "Platform.h"
#include "ExceptionHadling.h"
#include "SafeArithmetics.h"
#include "DataSerialization.h"
#include "HashedString.h"
#include "StrongType.h"
#include "TimeRelated.h"
#include "ExtendedTypes.h"
#include "Compressor.h"
#include "WorkThread.h"
#include "Logging.h"
#include "CommonHelpers.h"
// clang-format on

FO_BEGIN_NAMESPACE();

#define FO_NON_CONST_METHOD_HINT() _nonConstHelper = !_nonConstHelper
#define FO_NON_CONST_METHOD_HINT_ONELINE() _nonConstHelper = !_nonConstHelper;
#define FO_NON_NULL // Pointer annotation

///@ ExportValueType ident ident_t HardStrong HasValueAccessor Layout = int64-value
#define FO_IDENT_NAME "ident"
struct ident_t_traits
{
    static constexpr string_view name = FO_IDENT_NAME;
    static constexpr string_view underlying_type_name = "int64";
    using underlying_type = int64;
};
using ident_t = strong_type<ident_t_traits>;
static_assert(sizeof(ident_t) == sizeof(ident_t_traits::underlying_type));
static_assert(std::is_standard_layout_v<ident_t>);

// Custom any as string
// Todo: export any_t with ExportType
class any_t : public string
{
};

FO_END_NAMESPACE();
template<>
struct std::formatter<FO_NAMESPACE any_t> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE any_t& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(static_cast<const FO_NAMESPACE string&>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// 3d math types
// Todo: replace depedency from Assimp types (matrix/vector/quaternion/color)
FO_END_NAMESPACE();
#include "assimp/types.h"
FO_BEGIN_NAMESPACE();
using vec3 = aiVector3t<float32>;
using dvec3 = aiVector3t<float64>;
using mat44 = aiMatrix4x4t<float32>;
using dmat44 = aiMatrix4x4t<float64>;
using quaternion = aiQuaterniont<float32>;
using dquaternion = aiQuaterniont<float64>;
using color4 = aiColor4t<float32>;
using dcolor4 = aiColor4t<float64>;

FO_DECLARE_EXCEPTION(NotEnabled3DException);

// Atomic formatter
template<typename T>
auto constexpr is_atomic_v = is_specialization<T, std::atomic>::value;

FO_END_NAMESPACE();
template<typename T>
struct std::formatter<T, std::enable_if_t<FO_NAMESPACE is_atomic_v<T>, char>> : formatter<decltype(std::declval<T>().load())>
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<decltype(std::declval<T>().load())>::format(value.load(), ctx);
    }
};
FO_BEGIN_NAMESPACE();

// Event system
class EventUnsubscriberCallback final
{
    template<typename...>
    friend class EventObserver;
    friend class EventUnsubscriber;

public:
    EventUnsubscriberCallback() = delete;
    EventUnsubscriberCallback(const EventUnsubscriberCallback&) = delete;
    EventUnsubscriberCallback(EventUnsubscriberCallback&&) noexcept = default;
    auto operator=(const EventUnsubscriberCallback&) = delete;
    auto operator=(EventUnsubscriberCallback&&) noexcept -> EventUnsubscriberCallback& = default;
    ~EventUnsubscriberCallback() = default;

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriberCallback(Callback cb) noexcept :
        _unsubscribeCallback {std::move(cb)}
    {
    }
    Callback _unsubscribeCallback {};
};

class EventUnsubscriber final
{
    template<typename...>
    friend class EventObserver;

public:
    EventUnsubscriber() noexcept = default;
    EventUnsubscriber(const EventUnsubscriber&) = delete;
    EventUnsubscriber(EventUnsubscriber&&) noexcept = default;
    auto operator=(const EventUnsubscriber&) = delete;
    auto operator=(EventUnsubscriber&&) noexcept -> EventUnsubscriber& = default;
    ~EventUnsubscriber() { Unsubscribe(); }

    auto operator+=(EventUnsubscriberCallback&& cb) noexcept -> EventUnsubscriber&
    {
        _unsubscribeCallbacks.emplace_back(std::move(cb));
        return *this;
    }

    void Unsubscribe() noexcept
    {
        const auto callbacks = std::move(_unsubscribeCallbacks);
        _unsubscribeCallbacks.clear();

        for (const auto& cb : callbacks) {
            try {
                cb._unsubscribeCallback();
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        }
    }

private:
    using Callback = std::function<void()>;
    explicit EventUnsubscriber(EventUnsubscriberCallback cb) noexcept { _unsubscribeCallbacks.emplace_back(std::move(cb)); }
    vector<EventUnsubscriberCallback> _unsubscribeCallbacks {};
};

template<typename... Args>
class EventObserver final
{
    template<typename...>
    friend class EventDispatcher;

public:
    using Callback = std::function<void(Args...)>;

    EventObserver() = default;
    EventObserver(const EventObserver&) = delete;
    EventObserver(EventObserver&&) noexcept = default;
    auto operator=(const EventObserver&) = delete;
    auto operator=(EventObserver&&) noexcept = delete;

    ~EventObserver()
    {
        if (!_subscriberCallbacks.empty()) {
            try {
                throw GenericException("Some of subscriber still subscribed", _subscriberCallbacks.size());
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (...) {
                FO_UNKNOWN_EXCEPTION();
            }
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) noexcept -> EventUnsubscriberCallback
    {
        auto it = _subscriberCallbacks.insert(_subscriberCallbacks.end(), cb);
        return EventUnsubscriberCallback([this, it]() { _subscriberCallbacks.erase(it); });
    }

private:
    list<Callback> _subscriberCallbacks {};
};

template<typename... Args>
class EventDispatcher final
{
public:
    using ObserverType = EventObserver<Args...>;

    EventDispatcher() = delete;
    explicit EventDispatcher(ObserverType& obs) :
        _observer {obs}
    {
    }
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) noexcept = default;
    auto operator=(const EventDispatcher&) = delete;
    auto operator=(EventDispatcher&&) noexcept -> EventDispatcher& = default;
    ~EventDispatcher() = default;

    auto operator()(Args&&... args) -> EventDispatcher&
    {
        // Todo: recursion guard for EventDispatcher
        if (!_observer._subscriberCallbacks.empty()) {
            for (auto& cb : _observer._subscriberCallbacks) {
                cb(std::forward<Args>(args)...);
            }
        }
        return *this;
    }

private:
    ObserverType& _observer;
};

// Plain data detector
template<typename T>
constexpr bool is_valid_pod_type_v = std::is_standard_layout_v<T> && !is_strong_type_v<T> && !std::is_same_v<T, any_t> && //
    !std::is_same_v<T, string> && !std::is_same_v<T, string_view> && !std::is_same_v<T, hstring> && !std::is_arithmetic_v<T> && //
    !std::is_enum_v<T> && !is_specialization<T, vector>::value && !is_specialization<T, map>::value && !is_vector_v<T> && !is_map_v<T>;

// Generic constants
static constexpr auto LOCAL_CONFIG_NAME = "LocalSettings.focfg";
static constexpr auto PROCESS_TALK_TIME = 1000;
static constexpr float32 MIN_ZOOM = 0.1f;
static constexpr float32 MAX_ZOOM = 20.0f;

// Coordinates
static constexpr uint16 MAXHEX_DEFAULT = 200;
static constexpr uint16 MAXHEX_MIN = 10;
static constexpr uint16 MAXHEX_MAX = 4000;

// Answer
static constexpr uint8 ANSWER_BEGIN = 0xF0;
static constexpr uint8 ANSWER_END = 0xF1;
static constexpr uint8 ANSWER_BARTER = 0xF2;

// Look checks
static constexpr uint32 LOOK_CHECK_DIR = 0x01;
static constexpr uint32 LOOK_CHECK_SNEAK_DIR = 0x02;
static constexpr uint32 LOOK_CHECK_TRACE = 0x08;
static constexpr uint32 LOOK_CHECK_SCRIPT = 0x10;
static constexpr uint32 LOOK_CHECK_ITEM_SCRIPT = 0x20;
static constexpr uint32 LOOK_CHECK_TRACE_CLIENT = 0x40;

// Property type in network interaction
enum class NetProperty : uint8
{
    None = 0,
    Game, // No extra args
    Player, // No extra args
    Critter, // One extra arg: cr_id
    Chosen, // No extra args
    MapItem, // One extra arg: item_id
    CritterItem, // Two extra args: cr_id item_id
    ChosenItem, // One extra arg: item_id
    Map, // No extra args
    Location, // No extra args
    CustomEntity, // One extra arg: id
};

// Generic fixed game settings
struct GameSettings
{
#if FO_GEOMETRY == 1
    static constexpr bool HEXAGONAL_GEOMETRY = true;
    static constexpr bool SQUARE_GEOMETRY = false;
    static constexpr int32 MAP_DIR_COUNT = 6;
#elif FO_GEOMETRY == 2
    static constexpr bool HEXAGONAL_GEOMETRY = false;
    static constexpr bool SQUARE_GEOMETRY = true;
    static constexpr int32 MAP_DIR_COUNT = 8;
#else
#error FO_GEOMETRY not specified
#endif
};

// Light
static constexpr auto LIGHT_DISABLE_DIR(int32 dir) -> uint8
{
    return numeric_cast<uint8>(1u << std::clamp(dir, 0, 5));
}
static constexpr uint8 LIGHT_GLOBAL = 0x40;
static constexpr uint8 LIGHT_INVERSE = 0x80;

// Access
static constexpr uint8 ACCESS_CLIENT = 0;
static constexpr uint8 ACCESS_TESTER = 1;
static constexpr uint8 ACCESS_MODER = 2;
static constexpr uint8 ACCESS_ADMIN = 3;

// Commands
static constexpr auto CMD_EXIT = 1;
static constexpr auto CMD_MYINFO = 2;
static constexpr auto CMD_GAMEINFO = 3;
static constexpr auto CMD_CRITID = 4;
static constexpr auto CMD_MOVECRIT = 5;
static constexpr auto CMD_DISCONCRIT = 7;
static constexpr auto CMD_TOGLOBAL = 8;
static constexpr auto CMD_PROPERTY = 10;
static constexpr auto CMD_GETACCESS = 11;
static constexpr auto CMD_ADDITEM = 12;
static constexpr auto CMD_ADDITEM_SELF = 14;
static constexpr auto CMD_ADDNPC = 15;
static constexpr auto CMD_ADDLOCATION = 16;
static constexpr auto CMD_RUNSCRIPT = 20;
static constexpr auto CMD_REGENMAP = 25;
static constexpr auto CMD_LOG = 37;

///@ ExportEnum
enum class EngineInfoMessage : uint16
{
    None = 0,

    BarterNoBarterNow = 486,
    DialogNpcNotLife = 801,
    DialogDistTooLong = 803,
    DialogFromLinkNotFound = 807,
    DialogCompileFail = 808,
    DialogNpcNotFound = 809,
    DialogManyTalkers = 805,

    NetWrongLogin = 1001,
    NetWrongPass = 1002,
    NetPlayerAlready = 1003,
    NetPlayerInGame = 1004,
    NetWrongSpecial = 1005,
    NetRegSuccess = 1006,
    NetConnection = 1007,
    NetConnError = 1008,
    NetLoginPassWrong = 1009,
    NetConnSuccess = 1010,
    NetHexesBusy = 1012,
    NetDisconnByDemand = 1013,
    NetWrongName = 1014,
    NetWrongCases = 1015,
    NetWrongGender = 1016,
    NetWrongAge = 1017,
    NetConnFail = 1018,
    NetWrongData = 1019,
    NetStartLocFail = 1020,
    NetStartMapFail = 1021,
    NetStartCoordFail = 1022,
    NetBdError = 1023,
    NetWrongNetProto = 1024,
    NetDataTransErr = 1025,
    NetNetMsgErr = 1026,
    NetSetProtoErr = 1027,
    NetLoginOk = 1028,
    NetWrongTagSkill = 1029,
    NetDifferentLang = 1030,
    NetManySymbols = 1031,
    NetBeginEndSpaces = 1032,
    NetTwoSpace = 1033,
    NetBanned = 1034,
    NetNameWrongChars = 1035,
    NetPassWrongChars = 1036,
    NetFailToLoadIface = 1037,
    NetFailRunStartScript = 1038,
    NetLanguageNotSupported = 1039,
    NetKnockKnock = 1041,
    NetRegistrationIpWait = 1042,
    NetBannedIp = 1043,
    NetTimeLeft = 1045,
    NetBan = 1046,
    NetBanReason = 1047,
    NetLoginScriptFail = 1048,
    NetPermanentDeath = 1049,

    KickedFromGame = 5000,
    ServerLog = 5001,
};

// Network messages
enum class NetMessage : uint8
{
    Handshake = 1,
    Disconnect = 2,
    HandshakeAnswer = 3,
    InitData = 4,
    Login = 6,
    LoginSuccess = 7,
    Register = 11,
    RegisterSuccess = 13,
    Ping = 15,
    PlaceToGameComplete = 17,
    GetUpdateFile = 19,
    GetUpdateFileData = 21,
    UpdateFileData = 23,
    AddCritter = 25,
    RemoveCritter = 27,
    SendCommand = 28,
    InfoMessage = 32,
    SendCritterDir = 41,
    CritterDir = 42,
    SendCritterMove = 45,
    SendStopCritterMove = 46,
    CritterMove = 47,
    CritterMoveSpeed = 48,
    CritterPos = 49,
    CritterAttachments = 50,
    CritterTeleport = 52,
    ChosenAddItem = 65,
    ChosenRemoveItem = 66,
    AddItemOnMap = 71,
    RemoveItemFromMap = 74,
    AnimateItem = 75,
    SomeItems = 83,
    CritterAction = 91,
    CritterMoveItem = 93,
    CritterAnimate = 95,
    CritterSetAnims = 96,
    Effect = 100,
    FlyEffect = 101,
    PlaySound = 102,
    SendTalkNpc = 103,
    TalkNpc = 105,
    TimeSync = 107,
    LoadMap = 109,
    RemoteCall = 111,
    ViewMap = 115,
    AddCustomEntity = 116,
    RemoveCustomEntity = 117,
    Property = 120,
    SendProperty = 121,
};

struct BaseTypeInfo
{
    using StructLayoutEntry = pair<string, BaseTypeInfo>;
    using StructLayoutInfo = std::vector<StructLayoutEntry>;

    string TypeName {};
    bool IsString {};
    bool IsHash {};
    bool IsEnum {};
    bool IsEnumSigned {};
    bool IsPrimitive {}; // IsInt or IsFloat or IsBool
    bool IsInt {};
    bool IsSignedInt {};
    bool IsInt8 {};
    bool IsInt16 {};
    bool IsInt32 {};
    bool IsInt64 {};
    bool IsUInt8 {};
    bool IsUInt16 {};
    bool IsUInt32 {};
    bool IsUInt64 {};
    bool IsFloat {};
    bool IsSingleFloat {};
    bool IsDoubleFloat {};
    bool IsBool {};
    bool IsStruct {};
    const StructLayoutInfo* StructLayout {};
    size_t Size {};
};

FO_DECLARE_EXCEPTION(EnumResolveException);

class NameResolver
{
public:
    virtual ~NameResolver() = default;
    [[nodiscard]] virtual auto ResolveBaseType(string_view type_str) const -> BaseTypeInfo = 0;
    [[nodiscard]] virtual auto GetEnumInfo(string_view enum_name, const BaseTypeInfo** underlying_type) const -> bool = 0;
    [[nodiscard]] virtual auto GetValueTypeInfo(string_view type_name, size_t& size, const BaseTypeInfo::StructLayoutInfo** layout) const -> bool = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(string_view enum_name, int32 value, bool* failed = nullptr) const -> const string& = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(string_view str, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> = 0;
};

class FrameBalancer
{
public:
    FrameBalancer() = default;
    FrameBalancer(bool enabled, int32 sleep, int32 fixed_fps);

    [[nodiscard]] auto GetLoopDuration() const -> timespan { return _loopDuration; }

    void StartLoop();
    void EndLoop();

private:
    bool _enabled {};
    int32 _sleep {};
    int32 _fixedFps {};
    nanotime _loopStart {};
    timespan _loopDuration {};
    timespan _idleTimeBalance {};
};

FO_DECLARE_EXCEPTION(InfinityLoopException);

class InfinityLoopDetector
{
public:
    explicit InfinityLoopDetector(size_t max_count = 10) :
        _maxCount {max_count + 10}
    {
    }

    auto AddLoop() -> bool
    {
        if (++_counter >= _maxCount) {
            throw InfinityLoopException("Detected infinity loop", _counter);
        }

        return true;
    }

private:
    size_t _maxCount;
    size_t _counter {};
};

class GenericUtils final
{
public:
    GenericUtils() = delete;

    [[nodiscard]] static auto Random(int32 minimum, int32 maximum) -> int32;
    [[nodiscard]] static auto Percent(int32 full, int32 peace) -> int32;
    [[nodiscard]] static auto NumericalNumber(int32 num) noexcept -> int32;
    [[nodiscard]] static auto IntersectCircleLine(int32 cx, int32 cy, int32 radius, int32 x1, int32 y1, int32 x2, int32 y2) noexcept -> bool;
    [[nodiscard]] static auto DistSqrt(ipos pos1, ipos pos2) -> int32;
    [[nodiscard]] static auto GetStepsCoords(ipos from_pos, ipos to_pos) noexcept -> fpos;
    [[nodiscard]] static auto ChangeStepsCoords(fpos pos, float32 deq) noexcept -> fpos;

    static void SetRandomSeed(int32 seed);
    static void WriteSimpleTga(string_view fname, isize size, vector<ucolor> data);
};

// Interthread communication between server and client
using InterthreadDataCallback = std::function<void(const_span<uint8>)>;
extern map<uint16, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

FO_END_NAMESPACE();

#endif // FO_PRECOMPILED_HEADER_GUARD
