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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage

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

FO_BEGIN_NAMESPACE

// Force change of compatability version
///@ MigrationRule Version 0 0 3

#include "Version-Include.h"

extern auto IsPackaged() -> bool;
extern void ForcePackaged();

#define FO_NON_CONST_METHOD_HINT() _nonConstHelper = !_nonConstHelper
#define FO_NON_CONST_METHOD_HINT_ONELINE() _nonConstHelper = !_nonConstHelper;
#define FO_NON_NULL // Pointer annotation
#define FO_DEFERRED // Lambda annotation

///@ ExportValueType ident ident_t Layout = int64-value
using ident_t = strong_type<int64, struct ident_t_, strong_type_bool_test_tag>;
static_assert(some_strong_type<ident_t>);

// Custom any as string
// Todo: export any_t with ExportValueType
class any_t : public string
{
public:
    any_t() = default;
    explicit any_t(string other) :
        string(std::move(other))
    {
    }
};

FO_END_NAMESPACE
template<>
struct std::formatter<FO_NAMESPACE any_t> : formatter<FO_NAMESPACE string_view>
{
    template<typename FormatContext>
    auto format(const FO_NAMESPACE any_t& value, FormatContext& ctx) const
    {
        return formatter<FO_NAMESPACE string_view>::format(static_cast<const FO_NAMESPACE string&>(value), ctx);
    }
};
FO_BEGIN_NAMESPACE

// 3d math types
// Todo: replace depedency from Assimp types (matrix/vector/quaternion/color)
FO_END_NAMESPACE
#include "assimp/types.h"
FO_BEGIN_NAMESPACE
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
concept some_atomic = specialization_of<T, std::atomic>;

FO_END_NAMESPACE
template<typename T>
    requires(FO_NAMESPACE some_atomic<T>)
struct std::formatter<T> : formatter<decltype(std::declval<T>().load())> // NOLINT(cert-dcl58-cpp)
{
    template<typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const
    {
        return formatter<decltype(std::declval<T>().load())>::format(value.load(), ctx);
    }
};
FO_BEGIN_NAMESPACE

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
    using Callback = function<void()>;
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
        }
    }

private:
    using Callback = function<void()>;
    explicit EventUnsubscriber(EventUnsubscriberCallback cb) noexcept { _unsubscribeCallbacks.emplace_back(std::move(cb)); }
    vector<EventUnsubscriberCallback> _unsubscribeCallbacks {};
};

template<typename... Args>
class EventObserver final
{
    template<typename...>
    friend class EventDispatcher;

public:
    using Callback = function<void(Args...)>;

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
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) noexcept -> EventUnsubscriberCallback
    {
        auto it = _subscriberCallbacks.insert(_subscriberCallbacks.end(), cb);
        return EventUnsubscriberCallback([this, it]() FO_DEFERRED { _subscriberCallbacks.erase(it); });
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

// Valid plain data for properties
template<typename T>
concept some_property_plain_type = std::is_standard_layout_v<T> && !some_strong_type<T> && !std::same_as<T, any_t> && //
    !std::is_arithmetic_v<T> && !std::is_enum_v<T> && !vector_collection<T> && !map_collection<T> && //
    std::has_unique_object_representations_v<T> && !std::is_convertible_v<T, string_view>;
static_assert(!some_property_plain_type<string>);
static_assert(!some_property_plain_type<string_view>);
static_assert(!some_property_plain_type<hstring>);
static_assert(!some_property_plain_type<any_t>);

// Generic constants
static constexpr string_view_nt LOCAL_CONFIG_NAME = "LocalSettings.focfg";

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
    static constexpr float32 MIN_ZOOM = 0.05f;
    static constexpr float32 MAX_ZOOM = 20.0f;
    static constexpr int32 DEFAULT_MAP_SIZE = 200;
    static constexpr int32 MIN_MAP_SIZE = 10;
    static constexpr int32 MAX_MAP_SIZE = 4000;
};

///@ ExportEnum
enum class EngineInfoMessage : uint16
{
    None = 0,

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
    SomeItems = 83,
    CritterAction = 91,
    CritterMoveItem = 93,
    TimeSync = 107,
    LoadMap = 109,
    RemoteCall = 111,
    ViewMap = 115,
    AddCustomEntity = 116,
    RemoveCustomEntity = 117,
    Property = 120,
    SendProperty = 121,
};

enum class EngineSideKind : uint8
{
    ServerSide,
    ClientSide,
    MapperSide,
};

static constexpr size_t MAX_CALL_ARGS = 16;

struct StructLayoutDesc;
struct RefTypeDesc;

struct BaseTypeDesc
{
    auto operator==(const BaseTypeDesc& other) const noexcept -> bool { return Name == other.Name; }
    string Name {};
    bool IsObject {}; // entity, string, hstring, any, structs, ref types
    bool IsEntity {};
    bool IsGlobalEntity {};
    bool IsString {}; // string, any
    bool IsHashedString {};
    bool IsEnum {};
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
    bool IsSimpleStruct {}; // Layout size is one primitive type
    bool IsComplexStruct {}; // Layout more than one primitive
    bool IsRefType {};
    raw_ptr<const BaseTypeDesc> EnumUnderlyingType {};
    raw_ptr<const StructLayoutDesc> StructLayout {};
    raw_ptr<const RefTypeDesc> RefType {};
    size_t Size {};
};

enum class ComplexTypeKind : uint8
{
    None = 0,
    Simple = 1,
    Array = 2,
    Dict = 3,
    DictOfArray = 4,
    Callback = 5,
};

struct ComplexTypeDesc
{
    explicit operator bool() const noexcept { return Kind != ComplexTypeKind::None; }
    auto operator==(const ComplexTypeDesc&) const noexcept -> bool = default;
    ComplexTypeKind Kind {};
    BaseTypeDesc BaseType {};
    optional<BaseTypeDesc> KeyType {};
    shared_ptr<vector<ComplexTypeDesc>> CallbackArgs {};
    bool IsMutable {};
};

struct ArgDesc
{
    string Name {};
    ComplexTypeDesc Type {};
};

struct FieldDesc
{
    string Name {};
    BaseTypeDesc Type {};
    size_t Offset {};
};

struct FuncCallData;

struct MethodDesc
{
    using CallType = void (*)(FuncCallData&);

    string Name {};
    vector<ArgDesc> Args {};
    ComplexTypeDesc Ret {};
    CallType Call {};
    bool Getter {};
    bool Setter {};
    bool PassOwnership {};
};

struct StructLayoutDesc
{
    vector<FieldDesc> Fields {};
    size_t Size {};
};

struct RefTypeDesc
{
    vector<MethodDesc> Methods {};
};

struct RemoteCallDesc
{
    hstring Name {};
    vector<ArgDesc> Args {};
    string SubsystemHint {}; // File extension: fos, cs
};

template<typename Fn>
void VisitBaseTypePrimitive(void* p, const BaseTypeDesc& type, const Fn& fn)
{
    FO_RUNTIME_ASSERT(p);

    if (type.IsBool) {
        fn(*cast_from_void<bool*>(p));
        return;
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<int8*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<int16*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<int32*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<int64*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<uint8*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<uint16*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<uint32*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<uint64*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            fn(*cast_from_void<float32*>(p));
            return;
        }
        else if (type.IsDoubleFloat) {
            fn(*cast_from_void<float64*>(p));
            return;
        }
    }
    else if (type.IsEnum) {
        VisitBaseTypePrimitive(p, *type.EnumUnderlyingType, fn);
        return;
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            VisitBaseTypePrimitive(cast_from_void<uint8*>(p) + field.Offset, field.Type, fn);
        }

        return;
    }

    throw GenericException("Type is not visitable", type.Name);
}

template<typename Fn>
void VisitBaseTypePrimitive(const void* p, const BaseTypeDesc& type, const Fn& fn)
{
    FO_RUNTIME_ASSERT(p);

    if (type.IsBool) {
        fn(*cast_from_void<const bool*>(p));
        return;
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<const int8*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<const int16*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<const int32*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<const int64*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<const uint8*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<const uint16*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<const uint32*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<const uint64*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            fn(*cast_from_void<const float32*>(p));
            return;
        }
        else if (type.IsDoubleFloat) {
            fn(*cast_from_void<const float64*>(p));
            return;
        }
    }
    else if (type.IsEnum) {
        VisitBaseTypePrimitive(p, *type.EnumUnderlyingType, fn);
        return;
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            VisitBaseTypePrimitive(cast_from_void<const uint8*>(p) + field.Offset, field.Type, fn);
        }

        return;
    }

    throw GenericException("Type is not visitable", type.Name);
}

template<typename Fn>
decltype(auto) VisitBaseTypePrimitive(const void* a, const void* b, const BaseTypeDesc& type, Fn&& fn)
{
    FO_RUNTIME_ASSERT(a);
    FO_RUNTIME_ASSERT(b);

    if (type.IsBool) {
        return fn(*cast_from_void<const bool*>(a), *cast_from_void<const bool*>(b));
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            return fn(*cast_from_void<const int8*>(a), *cast_from_void<const int8*>(b));
        }
        case 2: {
            return fn(*cast_from_void<const int16*>(a), *cast_from_void<const int16*>(b));
        }
        case 4: {
            return fn(*cast_from_void<const int32*>(a), *cast_from_void<const int32*>(b));
        }
        case 8: {
            return fn(*cast_from_void<const int64*>(a), *cast_from_void<const int64*>(b));
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            return fn(*cast_from_void<const uint8*>(a), *cast_from_void<const uint8*>(b));
        }
        case 2: {
            return fn(*cast_from_void<const uint16*>(a), *cast_from_void<const uint16*>(b));
        }
        case 4: {
            return fn(*cast_from_void<const uint32*>(a), *cast_from_void<const uint32*>(b));
        }
        case 8: {
            return fn(*cast_from_void<const uint64*>(a), *cast_from_void<const uint64*>(b));
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            return fn(*cast_from_void<const float32*>(a), *cast_from_void<const float32*>(b));
        }
        else if (type.IsDoubleFloat) {
            return fn(*cast_from_void<const float64*>(a), *cast_from_void<const float64*>(b));
        }
    }
    else if (type.IsEnum) {
        return VisitBaseTypePrimitive(a, b, *type.EnumUnderlyingType, std::forward<Fn>(fn));
    }
    else if (type.IsSimpleStruct) {
        const auto& field = type.StructLayout->Fields.front();
        return VisitBaseTypePrimitive(a, b, field.Type, std::forward<Fn>(fn));
    }

    throw GenericException("Type is not binary visitable", type.Name);
}

FO_DECLARE_EXCEPTION(TypeResolveException);
FO_DECLARE_EXCEPTION(EnumResolveException);

class NameResolver
{
public:
    [[nodiscard]] virtual auto GetBaseType(string_view type_str) const -> const BaseTypeDesc& = 0;
    [[nodiscard]] virtual auto ResolveComplexType(string_view type_str) const -> ComplexTypeDesc = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_value_name, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_name, string_view value_name, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(string_view enum_name, int32 value, bool* failed = nullptr) const -> const string& = 0;
    [[nodiscard]] virtual auto ResolveGenericValue(string_view str, bool* failed = nullptr) const -> int32 = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> = 0;
    virtual ~NameResolver() = default;
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
    [[nodiscard]] static auto IntersectCircleLine(int32 cx, int32 cy, int32 radius, int32 x1, int32 y1, int32 x2, int32 y2) noexcept -> bool;
    [[nodiscard]] static auto GetStepsCoords(ipos32 from_pos, ipos32 to_pos) noexcept -> fpos32;
    [[nodiscard]] static auto ChangeStepsCoords(fpos32 pos, float32 deq) noexcept -> fpos32;

    static void SetRandomSeed(int32 seed);
    static void WriteSimpleTga(string_view fname, isize32 size, vector<ucolor> data);
};

// Interthread communication between server and client
using InterthreadDataCallback = function<void(span<const uint8>)>;
extern map<uint16, function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

///@ ExportEnum
enum class CritterItemSlot : uint8
{
    Inventory = 0,
    Main = 1,
    Outside = 255,
};

///@ ExportEnum
enum class CritterCondition : uint8
{
    Alive = 0,
    Knockout = 1,
    Dead = 2,
};

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//  flags actionExt item
///@ ExportEnum
enum class CritterAction : uint16
{
    None = 0,
    MoveItem = 2,
    SwapItems = 3,
    DropItem = 5,
    Knockout = 16,
    StandUp = 17,
    Dead = 19,
    Connect = 20,
    Disconnect = 21,
    Respawn = 22,
    Refresh = 23,
};

///@ ExportEnum
enum class CritterStateAnim : uint16
{
    None = 0,
    Unarmed = 1,
};

///@ ExportEnum
enum class CritterActionAnim : uint16
{
    None = 0,
    Idle = 1,
    Walk = 3,
    WalkBack = 15,
    Limp = 4,
    Run = 5,
    RunBack = 16,
    TurnRight = 17,
    TurnLeft = 18,
    PanicRun = 6,
    SneakWalk = 7,
    SneakRun = 8,
    IdleProneFront = 86,
    DeadFront = 102,
};

///@ ExportEnum
enum class CritterSeeType : uint8
{
    Any = 0,
    WhoSeeMe = 1,
    WhoISee = 2,
};

///@ ExportEnum
enum class CritterFindType : uint8
{
    Any = 0,
    NonDead = 0x01,
    Dead = 0x02,
    Players = 0x10,
    Npc = 0x20,
    NonDeadPlayers = 0x11,
    DeadPlayers = 0x12,
    NonDeadNpc = 0x21,
    DeadNpc = 0x22,
};

///@ ExportEnum
enum class ItemOwnership : uint8
{
    MapHex = 0,
    CritterInventory = 1,
    ItemContainer = 2,
    Nowhere = 3,
};

///@ ExportEnum
enum class CornerType : uint8
{
    NorthSouth = 0,
    West = 1,
    East = 2,
    South = 3,
    North = 4,
    EastWest = 5,
};

///@ ExportEnum
enum class MultihexGenerationType : uint8
{
    None = 0,
    SameSibling = 1,
    AnyUnique = 2,
};

class AnimationResolver
{
public:
    [[nodiscard]] virtual auto ResolveCritterAnimationFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& pass, uint32& flags, int32& ox, int32& oy, string& anim_name) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32& f_state_anim, int32& f_action_anim, int32& f_state_anim_ex, int32& f_action_anim_ex, uint32& flags) -> bool = 0;
    virtual ~AnimationResolver() = default;
};

FO_END_NAMESPACE

#endif // FO_PRECOMPILED_HEADER_GUARD
