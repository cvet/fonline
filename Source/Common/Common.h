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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Essentials.h"

FO_BEGIN_NAMESPACE

// The native-codegen surface is offered for evaluation only, and stays revision-pinned until supported release lines exist.
// SymbolCount and InventorySha256 force owner review of every addition, removal or stable-ID change
///@ ApiContract scope:native-codegen experimental Since=2022.1.0.wip SymbolCount=2574 InventorySha256=c550a5295e1786447151336e94258036c65c2fe6ec55d2c7fc0b60636fb03998

// Force change of compatability version
///@ MigrationRule Version 0 0 68

auto IsPackaged() -> bool;
auto GetPackagedRuntimeName() -> string;
extern bool IsTestingInProgress;

#define FO_DEFERRED // Lambda annotation

// Every entity method declares its call-time preconditions with FO_VALIDATE_ENTITY(<flags>); the FO_VE_CHECK_*
// macros below name each flag and what it does on violation
class Entity;
inline void ValidateEntityAccess(nptr<const Entity> entity);

#define FO_VE_CHECK_LOCKED ValidateEntityAccess(this);
#define FO_VE_CHECK_NOT_DESTROYING FO_VERIFY_AND_THROW(!this->IsDestroying(), "Entity method called while the entity is being destroyed", this->GetName());
#define FO_VE_CHECK_NOT_DESTROYED FO_STRONG_ASSERT(!this->IsDestroyed(), "Entity method called on an already destroyed entity", this->GetName());
#define FO_VE_CHECK_NONE
#define FO_VE_DISPATCH(flag) FO_CONCAT(FO_VE_CHECK_, flag)

// Bounded variadic dispatch (1..3 flags); the FO_VE_EXPAND wrapper keeps it correct on the MSVC preprocessor
#define FO_VE_EXPAND(x) x
#define FO_VE_NARGS(...) FO_VE_EXPAND(FO_VE_NARGS_IMPL(__VA_ARGS__, 3, 2, 1))
#define FO_VE_NARGS_IMPL(_1, _2, _3, N, ...) N
#define FO_VE_FOREACH_1(m, a) m(a)
#define FO_VE_FOREACH_2(m, a, b) m(a) m(b)
#define FO_VE_FOREACH_3(m, a, b, c) m(a) m(b) m(c)
#define FO_VE_FOREACH(m, ...) FO_VE_EXPAND(FO_CONCAT(FO_VE_FOREACH_, FO_VE_NARGS(__VA_ARGS__))(m, __VA_ARGS__))

#define FO_VALIDATE_ENTITY(...) FO_VE_FOREACH(FO_VE_DISPATCH, __VA_ARGS__)

// Explicit-entity access check (validates a passed-in entity argument rather than `this`)
#define FO_VALIDATE_ENTITY_ACCESS_VALUE(entity) ValidateEntityAccess(entity)

// Strong signed 64-bit identity token whose zero value is false and whose ordering compares the stored value
// value: Signed 64-bit identity payload; zero represents an empty identity
///@ ExportValueType Name = ident Layout = int64-value
using ident_t = strong_type<int64_t, struct ident_t_, strong_type_bool_test_tag, strong_type_sortings_tag>;
static_assert(some_strong_type<ident_t>);

// Command line arguments
using CommandLineArg = nptr<char>;

class CommandLineArgs
{
public:
    CommandLineArgs() = default;
    explicit CommandLineArgs(int32_t argc, nptr<char*> argv)
    {
        size_t arg_count = numeric_cast<size_t>(argc);
        FO_VERIFY_AND_THROW(arg_count == 0 || argv, "Command line argument vector is null while argument count is non-zero");

        _args.resize(arg_count);

        for (size_t i = 0; i < arg_count; ++i) {
            FO_VERIFY_AND_THROW(argv[i] != nullptr, "Command line argument string is null");
            _args[i] = argv[i];
        }
    }
    explicit CommandLineArgs(const_span<CommandLineArg> args) :
        _args(args.begin(), args.end())
    {
        for (CommandLineArg arg : _args) {
            FO_VERIFY_AND_THROW(arg, "Command line argument string is null");
        }
    }

    // An option names a setting, so its name starts with a letter; a dash before anything else opens a value such as -1
    [[nodiscard]] static auto IsOption(string_view arg) noexcept -> bool
    {
        if (!arg.starts_with('-')) {
            return false;
        }

        string_view name = arg.substr(arg.starts_with("--") ? 2 : 1);

        if (name.empty()) {
            return false;
        }

        char first = name.front();
        return (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
    }
    [[nodiscard]] auto Get(size_t index) const noexcept -> string_view { return index < _args.size() ? string_view(_args[index].get()) : string_view(); }
    [[nodiscard]] auto size() const noexcept -> size_t { return _args.size(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return _args.empty(); }
    [[nodiscard]] auto operator[](size_t index) const -> CommandLineArg { return _args[index]; }
    [[nodiscard]] auto begin() const noexcept { return _args.begin(); }
    [[nodiscard]] auto end() const noexcept { return _args.end(); }

private:
    vector<CommandLineArg> _args {};
};

// Owns a process's arguments in UTF-8 for an entry point SDL does not run: on Windows its argv arrives in the ANSI
// code page, which cannot carry every path, so the arguments are read from the wide command line instead
class ProgramArgs final
{
public:
    ProgramArgs(int32_t argc, nptr<char*> argv);
    ProgramArgs(const ProgramArgs&) = delete;
    ProgramArgs(ProgramArgs&&) noexcept = delete;
    auto operator=(const ProgramArgs&) = delete;
    auto operator=(ProgramArgs&&) noexcept = delete;
    ~ProgramArgs() = default;

    [[nodiscard]] auto GetArgs() const -> CommandLineArgs { return CommandLineArgs {_pointers}; }

private:
    vector<string> _values {};
    vector<CommandLineArg> _pointers {};
};

// Custom any as string
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
FO_END_NAMESPACE
#define GLM_FORCE_CTOR_INIT
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
FO_BEGIN_NAMESPACE
using vec3 = glm::vec<3, float32_t, glm::defaultp>;
using dvec3 = glm::vec<3, float64_t, glm::defaultp>;
using mat44 = glm::mat<4, 4, float32_t, glm::defaultp>;
using dmat44 = glm::mat<4, 4, float64_t, glm::defaultp>;
using quaternion = glm::qua<float32_t, glm::defaultp>;
using dquaternion = glm::qua<float64_t, glm::defaultp>;
using color4 = glm::vec<4, float32_t, glm::defaultp>;
using dcolor4 = glm::vec<4, float64_t, glm::defaultp>;

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
        auto callbacks = std::move(_unsubscribeCallbacks);
        _unsubscribeCallbacks.clear();

        for (const auto& cb : callbacks) {
            try {
                cb._unsubscribeCallback();
            }
            catch (const std::exception& ex) {
                exceptions::report_and_continue(ex);
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
                exceptions::report_and_continue(ex);
            }
        }
    }

    [[nodiscard]] auto operator+=(Callback cb) noexcept -> EventUnsubscriberCallback
    {
        auto it = _subscriberCallbacks.insert(_subscriberCallbacks.end(), std::move(cb));
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
    explicit EventDispatcher(ptr<ObserverType> obs) :
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
        if (!_observer->_subscriberCallbacks.empty()) {
            for (auto& cb : _observer->_subscriberCallbacks) {
                cb(args...);
            }
        }
        return *this;
    }

private:
    ptr<ObserverType> _observer;
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

// Property type in network interaction
enum class NetProperty : uint8_t
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
    static constexpr int32_t MAP_DIR_COUNT = 6;
#elif FO_GEOMETRY == 2
    static constexpr bool HEXAGONAL_GEOMETRY = false;
    static constexpr bool SQUARE_GEOMETRY = true;
    static constexpr int32_t MAP_DIR_COUNT = 8;
#else
#error FO_GEOMETRY not specified
#endif
    static constexpr int32_t MAP_HEX_WIDTH = FO_MAP_HEX_WIDTH;
    static constexpr int32_t MAP_HEX_HEIGHT = FO_MAP_HEX_HEIGHT;
    static constexpr int32_t MAP_HEX_LINE_HEIGHT = HEXAGONAL_GEOMETRY ? (MAP_HEX_HEIGHT * 3) / 4 : MAP_HEX_HEIGHT / 2;
    static constexpr float32_t MAP_CAMERA_ANGLE = const_numeric_cast<float32_t>(FO_MAP_CAMERA_ANGLE);
    static constexpr float32_t MIN_ZOOM = 0.05f;
    static constexpr float32_t MAX_ZOOM = 20.0f;
    static constexpr int32_t DEFAULT_MAP_SIZE = 200;
    static constexpr int32_t MIN_MAP_SIZE = 10;
    static constexpr int32_t MAX_MAP_SIZE = 4000;
};

// Stable text-message identifiers for connection, authentication, loading, and runtime status feedback. The generic info-message transport treats them as opaque values; embedding projects own their text and dispatch policy
///@ ExportEnum
enum class EngineInfoMessage : uint16_t
{
    None = 0, // Selects no conventional engine information-message slot

    NetWrongLogin = 1001, // Identifies a login rejection caused by an invalid account or login identifier
    NetWrongPass = 1002, // Identifies a login rejection caused by an invalid password or equivalent authentication secret
    NetPlayerAlready = 1003, // Identifies account creation rejected because the requested login already exists
    NetPlayerInGame = 1004, // Identifies login rejected because the account already has an active game session
    NetConnection = 1007, // Identifies the client status while it is initiating a server connection
    NetConnError = 1008, // Identifies a generic connection error after a connection attempt or active session failure
    NetConnSuccess = 1010, // Identifies successful transport connection before or during authentication
    NetHexesBusy = 1012, // Identifies entry or spawning rejected because the required destination hexes are occupied
    NetDisconnByDemand = 1013, // Identifies a disconnect requested explicitly by the player or client
    NetConnFail = 1018, // Identifies failure to establish a connection to the game server
    NetStartLocFail = 1020, // Identifies failure to resolve the starting location during player entry or creation
    NetStartMapFail = 1021, // Identifies failure to resolve the starting map during player entry or creation
    NetStartCoordFail = 1022, // Identifies failure to resolve starting map coordinates during player entry or creation
    NetBdError = 1023, // Identifies a backend database failure while processing the requested account or game operation
    NetWrongNetProto = 1024, // Identifies a client/server network-protocol version mismatch
    NetDataTransErr = 1025, // Identifies an error while transferring or decoding connection data
    NetNetMsgErr = 1026, // Identifies a malformed, invalid, or otherwise unacceptable network message
    NetSetProtoErr = 1027, // Identifies an internal server failure while assigning or initializing required prototype state
    NetLoginOk = 1028, // Identifies successful authentication and transition to game-state or map loading
    NetWrongTagSkill = 1029, // Identifies character creation rejected because the required tagged-skill selection is invalid
    NetDifferentLang = 1030, // Identifies a name rejected for mixing characters from different language alphabets
    NetManySymbols = 1031, // Identifies a name rejected because too much of it consists of non-letter symbols
    NetBeginEndSpaces = 1032, // Identifies a name rejected because it begins or ends with whitespace
    NetTwoSpace = 1033, // Identifies a name rejected because it contains consecutive spaces
    NetBanned = 1034, // Identifies an account that is blocked from logging in
    NetNameWrongChars = 1035, // Identifies a name containing characters forbidden by the project's account policy
    NetPassWrongChars = 1036, // Identifies a password containing characters forbidden by the project's account policy
    NetFailToLoadIface = 1037, // Identifies client startup failure while loading the user interface
    NetFailRunStartScript = 1038, // Identifies client startup failure while executing the project's start script
    NetLanguageNotSupported = 1039, // Identifies an unsupported selected language and the need to use a project-defined fallback
    NetKnockKnock = 1041, // Reserves the legacy knock-knock informational slot; embedding scripts define its concrete liveness or status use
    NetBannedIp = 1043, // Identifies a connection rejected because its source IP address is blocked
    NetTimeLeft = 1045, // Identifies a temporary restriction or session timer whose remaining duration is supplied by project text or extra data
    NetBan = 1046, // Identifies the primary notification that the player or account has been banned
    NetBanReason = 1047, // Identifies detailed ban metadata such as issuer, duration, and reason supplied by the project
    NetLoginScriptFail = 1048, // Identifies authentication rejected because a project login script failed
    NetPermanentDeath = 1049, // Identifies a project-defined permanent-death state that prevents continuing with the affected character or account

    KickedFromGame = 5000, // Identifies forced removal of the player from the active game session
    ServerLog = 5001, // Identifies a server-log message whose reader-facing payload is carried in extraText
};

static constexpr uint32_t FO_UPDATER_VERSION = 5;

enum class UpdatePlatform : uint8_t
{
    Unknown = 0,
    Windows = 1,
    Linux = 2,
    Android = 3,
    MacOS = 4,
    IOS = 5,
    Web = 6,
};

enum class UpdateFileTarget : uint8_t
{
    ClientResources = 0,
    ClientBinaries = 1,
};

// Network messages
enum class NetMessage : uint8_t
{
    Handshake = 1,
    Disconnect = 2,
    HandshakeAnswer = 3,
    InitData = 4,
    LoginSuccess = 7,
    Ping = 15,
    PlaceToGameComplete = 17,
    GetUpdateFile = 19,
    UpdateFileData = 23,
    AddCritter = 25,
    RemoveCritter = 27,
    InfoMessage = 32,
    SendCritterDir = 41,
    CritterDir = 42,
    SendCritterMoveFinished = 44,
    SendCritterMove = 45,
    SendStopCritterMove = 46,
    CritterMove = 47,
    CritterMoveSpeed = 48,
    CritterPos = 49,
    CritterAttachments = 50,
    CritterVisibilityMode = 51,
    CritterTeleport = 52,
    SendCritterMoveLease = 53,
    CritterMoveLease = 54,
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
    HashList = 122,
    UnresolvedHash = 123,
};

enum class EngineSideKind : uint8_t
{
    ServerSide,
    ClientSide,
    MapperSide,
};

static constexpr size_t MAX_CALL_ARGS = 16;

struct StructLayoutDesc;
struct RefTypeDesc;
class PropertyRegistrar;

struct BaseTypeDesc
{
    auto operator==(const BaseTypeDesc& other) const noexcept -> bool { return Name == other.Name; }
    string Name {};
    hstring HashedName {};
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
    bool IsSingleton {};
    bool IsFixedType {};
    bool IsEntityProto {};
    bool IsAbstractEntity {};
    nptr<const BaseTypeDesc> EnumUnderlyingType {};
    nptr<const StructLayoutDesc> StructLayout {};
    nptr<const RefTypeDesc> RefType {};
    size_t Size {};
};

enum class ComplexTypeKind : uint8_t
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

// Synchronization-cover markers for script exports. All expand to nothing: the compiler never sees them, codegen
// does
#define FO_REQUIRES_COVER
#define FO_PROVIDES_COVER
#define FO_RETURNS_PARENT
#define FO_RETURNS_ANCESTOR

// The raw synchronization surface, marked where it is exported: naming these in the analyzer would let a
// rename here disarm a rule with nothing left to notice it
#define FO_COVER_PRIMITIVE
#define FO_COVER_PROBE
#define FO_SINGLETON_LOCK

struct ArgDesc
{
    string Name {};
    ComplexTypeDesc Type {};
    bool Nullable {};
    string DefaultValue {};

    // The caller must already hold synchronization cover for this argument
    bool RequiresCover {};
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
    bool GlobalGetter {};
    bool Getter {};
    bool Setter {};
    bool PassOwnership {};
    bool ReturnNullable {};
    bool Async {};

    // A downward accessor: the entities it returns live under its receiver in the sync hierarchy, so the receiver's
    // cover already covers them. Declared with FO_PROVIDES_COVER before the return type
    bool ReturnProvidesCover {};

    // An upward accessor: it returns the receiver's sync-hierarchy parent (FO_RETURNS_PARENT) or some ancestor
    // (FO_RETURNS_ANCESTOR). The receiver's own cover does not reach it; cover declared with that reach does
    bool ReturnIsParent {};
    bool ReturnIsAncestor {};

    // The raw surface script code never reaches for directly: the primitive that replaces the held set, the
    // question about what is held, and the singleton bucket lock
    bool IsCoverPrimitive {};
    bool IsCoverProbe {};
    bool IsSingletonLock {};
};

// A value type (IsStruct) is plain data: packed fields, no tail padding, and a native twin of the same size that is
// trivially copyable, so every consumer moves it with memcpy. Anything holding complex data is a ref type instead
struct StructLayoutDesc
{
    size_t NativeSize {};
    vector<FieldDesc> Fields {};
    size_t Size {};
};

struct RefTypeDesc
{
    vector<MethodDesc> Methods {};
    nptr<const PropertyRegistrar> FieldsRegistrar {};
    bool IsDynamicLayout {};
};

struct RemoteCallDesc
{
    hstring Name {};
    vector<ArgDesc> Args {};
    string SubsystemHint {}; // File extension: fos, cs
    size_t MaxPayloadSize {}; // Structural wire limit generated from the RemoteCall declaration; 0 means unspecified
    size_t MaxCollectionSize {}; // Structural limit for every declared collection in this call; 0 means unspecified
};

auto GetRemoteCallSimpleValueMinWireSize(const BaseTypeDesc& type) -> size_t;

template<typename Fn>
void VisitBaseTypePrimitive(void* p, const BaseTypeDesc& type, const Fn& fn)
{
    FO_VERIFY_AND_THROW(p, "Value pointer is null");

    if (type.IsBool) {
        fn(*cast_from_void<bool*>(p));
        return;
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<int8_t*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<int16_t*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<int32_t*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<int64_t*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<uint8_t*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<uint16_t*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<uint32_t*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<uint64_t*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            fn(*cast_from_void<float32_t*>(p));
            return;
        }
        else if (type.IsDoubleFloat) {
            fn(*cast_from_void<float64_t*>(p));
            return;
        }
    }
    else if (type.IsHashedString) {
        fn(*cast_from_void<hstring*>(p));
        return;
    }
    else if (type.IsEnum) {
        VisitBaseTypePrimitive(p, *type.EnumUnderlyingType, fn);
        return;
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            VisitBaseTypePrimitive(cast_from_void<uint8_t*>(p).get() + field.Offset, field.Type, fn);
        }

        return;
    }

    throw GenericException("Type is not visitable", type.Name);
}

template<typename Fn>
void VisitBaseTypePrimitive(const void* p, const BaseTypeDesc& type, const Fn& fn)
{
    FO_VERIFY_AND_THROW(p, "Value pointer is null");

    if (type.IsBool) {
        fn(*cast_from_void<const bool*>(p));
        return;
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<const int8_t*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<const int16_t*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<const int32_t*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<const int64_t*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            fn(*cast_from_void<const uint8_t*>(p));
            return;
        }
        case 2: {
            fn(*cast_from_void<const uint16_t*>(p));
            return;
        }
        case 4: {
            fn(*cast_from_void<const uint32_t*>(p));
            return;
        }
        case 8: {
            fn(*cast_from_void<const uint64_t*>(p));
            return;
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            fn(*cast_from_void<const float32_t*>(p));
            return;
        }
        else if (type.IsDoubleFloat) {
            fn(*cast_from_void<const float64_t*>(p));
            return;
        }
    }
    else if (type.IsHashedString) {
        fn(*cast_from_void<const hstring*>(p));
        return;
    }
    else if (type.IsEnum) {
        VisitBaseTypePrimitive(p, *type.EnumUnderlyingType, fn);
        return;
    }
    else if (type.IsStruct) {
        for (const auto& field : type.StructLayout->Fields) {
            VisitBaseTypePrimitive(cast_from_void<const uint8_t*>(p).get() + field.Offset, field.Type, fn);
        }

        return;
    }

    throw GenericException("Type is not visitable", type.Name);
}

template<typename Fn>
decltype(auto) VisitBaseTypePrimitive(ptr<const void> a, ptr<const void> b, const BaseTypeDesc& type, Fn&& fn)
{
    if (type.IsBool) {
        return fn(*cast_from_void<const bool*>(a.get()), *cast_from_void<const bool*>(b.get()));
    }
    else if (type.IsInt && type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            return fn(*cast_from_void<const int8_t*>(a.get()), *cast_from_void<const int8_t*>(b.get()));
        }
        case 2: {
            return fn(*cast_from_void<const int16_t*>(a.get()), *cast_from_void<const int16_t*>(b.get()));
        }
        case 4: {
            return fn(*cast_from_void<const int32_t*>(a.get()), *cast_from_void<const int32_t*>(b.get()));
        }
        case 8: {
            return fn(*cast_from_void<const int64_t*>(a.get()), *cast_from_void<const int64_t*>(b.get()));
        }
        default:
            break;
        }
    }
    else if (type.IsInt && !type.IsSignedInt) {
        switch (type.Size) {
        case 1: {
            return fn(*cast_from_void<const uint8_t*>(a.get()), *cast_from_void<const uint8_t*>(b.get()));
        }
        case 2: {
            return fn(*cast_from_void<const uint16_t*>(a.get()), *cast_from_void<const uint16_t*>(b.get()));
        }
        case 4: {
            return fn(*cast_from_void<const uint32_t*>(a.get()), *cast_from_void<const uint32_t*>(b.get()));
        }
        case 8: {
            return fn(*cast_from_void<const uint64_t*>(a.get()), *cast_from_void<const uint64_t*>(b.get()));
        }
        default:
            break;
        }
    }
    else if (type.IsFloat) {
        if (type.IsSingleFloat) {
            return fn(*cast_from_void<const float32_t*>(a.get()), *cast_from_void<const float32_t*>(b.get()));
        }
        else if (type.IsDoubleFloat) {
            return fn(*cast_from_void<const float64_t*>(a.get()), *cast_from_void<const float64_t*>(b.get()));
        }
    }
    else if (type.IsHashedString) {
        return fn(*cast_from_void<const hstring*>(a.get()), *cast_from_void<const hstring*>(b.get()));
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

class ProtoEntity;

class NameResolver
{
public:
    [[nodiscard]] virtual auto GetBaseType(string_view type_str) const -> const BaseTypeDesc& = 0;
    [[nodiscard]] virtual auto ResolveComplexType(string_view type_str) const -> ComplexTypeDesc = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_value_name, nptr<bool> failed = nullptr) const -> int32_t = 0;
    [[nodiscard]] virtual auto ResolveEnumValue(string_view enum_name, string_view value_name, nptr<bool> failed = nullptr) const -> int32_t = 0;
    [[nodiscard]] virtual auto ResolveEnumValueName(string_view enum_name, int32_t value, nptr<bool> failed = nullptr) const -> string_view = 0;
    [[nodiscard]] virtual auto CheckMigrationRule(hstring rule_name, hstring extra_info, hstring target) const noexcept -> optional<hstring> = 0;
    [[nodiscard]] virtual auto GetProtoEntity(hstring type_name, hstring proto_id) const noexcept -> nptr<const ProtoEntity> = 0;
    virtual ~NameResolver() = default;
};

class FrameBalancer
{
public:
    FrameBalancer() = default;
    FrameBalancer(bool enabled, int32_t sleep, int32_t fixed_fps);

    void StartLoop();
    void EndLoop();

private:
    bool _enabled {};
    int32_t _sleep {};
    int32_t _fixedFps {};
    nanotime _loopStart {};
    timespan _loopDuration {};
    timespan _idleTimeBalance {};
};

// Interthread communication between server and client
using InterthreadDataCallback = function<void(span<const uint8_t>)>;
using InterthreadListener = copyable_function<InterthreadDataCallback(InterthreadDataCallback)>;

// One table for the process, keyed by virtual port, so an embedded client finds the server running beside it.
// Listeners are handed out by copy and called outside the table's lock
auto AddInterthreadListener(uint16_t port, InterthreadListener listener) -> bool;
auto RemoveInterthreadListener(uint16_t port) -> bool;
auto FindInterthreadListener(uint16_t port) -> optional<InterthreadListener>;
auto HasInterthreadListener(uint16_t port) -> bool;

// Logical critter item destinations used for inventory, equipped-main-slot, and outside-item transfers
///@ ExportEnum
enum class CritterItemSlot : uint8_t
{
    Inventory = 0, // Places the item in the critter's unequipped inventory
    Main = 1, // Places the item in the critter's main equipped slot
    Outside = 255, // Marks the item as outside the critter's owned inventory slots
};

// High-level life condition of a critter
///@ ExportEnum
enum class CritterCondition : uint8_t
{
    Alive = 0, // Critter is alive and may perform normal gameplay actions
    Knockout = 1, // Critter is alive but incapacitated until it stands up or changes condition
    Dead = 2, // Critter is dead and uses death-state handling and animation
};

// Engine-originated critter action notifications such as item movement, knockout, death, connection, and respawn.
// Some actions have hardcoded local or server dispatch rules; project code should consume the symbolic action
///@ ExportEnum
enum class CritterAction : uint16_t
{
    None = 0, // No critter action notification is selected
    MoveItem = 2, // Notifies observers that an item moved between critter slots
    SwapItems = 3, // Notifies observers about the second item participating in a slot swap
    DropItem = 5, // Notifies observers that the critter dropped an item from a slot
    Knockout = 16, // Notifies observers that the critter entered the knockout condition
    StandUp = 17, // Notifies observers that the critter recovered from knockout and stood up
    Dead = 19, // Notifies observers that the critter entered the dead condition
    Connect = 20, // Notifies observers that the player-controlled critter connected
    Disconnect = 21, // Notifies observers that the player-controlled critter disconnected
    Respawn = 22, // Notifies observers that the critter returned to the alive condition outside knockout recovery
    Refresh = 23, // Requests observers to refresh the critter's visual action state
};

// Persistent critter animation posture passed to model animation resolution
///@ ExportEnum
enum class CritterStateAnim : uint16_t
{
    None = 0, // No persistent critter animation posture is selected
    Unarmed = 1, // Selects the unarmed persistent animation posture
};

// Requested critter movement or pose animation passed to model animation resolution
///@ ExportEnum
enum class CritterActionAnim : uint16_t
{
    None = 0, // No action animation is requested
    Idle = 1, // Requests the standing idle animation
    Walk = 3, // Requests forward walking
    WalkBack = 15, // Requests backward walking
    Limp = 4, // Requests the limping movement animation
    Run = 5, // Requests forward running
    RunBack = 16, // Requests backward running
    TurnRight = 17, // Requests an in-place right turn
    TurnLeft = 18, // Requests an in-place left turn
    PanicRun = 6, // Requests the panic-running animation
    SneakWalk = 7, // Requests walking in the sneaking posture
    SneakRun = 8, // Requests running in the sneaking posture
    IdleProneFront = 86, // Requests the front-facing prone idle animation
    DeadFront = 102, // Requests the front-facing death animation
};

// Perspective used by critter visibility queries: either direction or their union
///@ ExportEnum
enum class CritterSeeType : uint8_t
{
    Any = 0, // Returns the union of incoming and outgoing critter visibility relations
    WhoSeeMe = 1, // Selects critters whose visibility relation currently includes this critter
    WhoISee = 2, // Selects critters currently visible to this critter
};

// Visibility override applied to a critter independently of normal perception checks
///@ ExportEnum
enum class CritterVisibilityMode : uint8_t
{
    None = 0, // Applies no full-visibility override and uses normal perception rules
    Full = 1, // Forces the target into full visibility for the selected relation
};

// Composable filters for selecting critters by life state and player-or-NPC ownership
///@ ExportEnum
enum class CritterFindType : uint8_t
{
    Any = 0, // Selects critters without filtering life state or player ownership
    NonDead = 0x01, // Selects alive and knocked-out critters while excluding dead critters
    Dead = 0x02, // Selects dead critters regardless of player ownership
    Players = 0x10, // Selects player-controlled critters regardless of life state
    Npc = 0x20, // Selects non-player critters regardless of life state
    NonDeadPlayers = 0x11, // Selects player-controlled critters that are not dead
    DeadPlayers = 0x12, // Selects dead player-controlled critters
    NonDeadNpc = 0x21, // Selects non-player critters that are not dead
    DeadNpc = 0x22, // Selects dead non-player critters
};

// Current ownership location of an item: map hex, critter inventory, item container, or no owner
///@ ExportEnum
enum class ItemOwnership : uint8_t
{
    MapHex = 0, // Item is placed directly on a map hex
    CritterInventory = 1, // Item is owned by a critter inventory or equipped slot
    ItemContainer = 2, // Item is nested inside another item used as a container
    Nowhere = 3, // Item has no map, critter, or item-container owner
};

// Wall-corner orientation used by map geometry and corner-aware rendering
///@ ExportEnum
enum class CornerType : uint8_t
{
    NorthSouth = 0, // Selects the combined north-south wall-corner orientation
    West = 1, // Selects the west-facing wall-corner orientation
    East = 2, // Selects the east-facing wall-corner orientation
    South = 3, // Selects the south-facing wall-corner orientation
    North = 4, // Selects the north-facing wall-corner orientation
    EastWest = 5, // Selects the combined east-west wall-corner orientation
};

// Policy for generating occupied hexes around a multihex prototype
///@ ExportEnum
enum class MultihexGenerationType : uint8_t
{
    None = 0, // Disables Mapper coalescing of item placements into a multihex mesh
    SameSibling = 1, // Coalesces spatially adjacent compatible sibling items into one incrementally grown multihex mesh
    AnyUnique = 2, // Coalesces compatible same-prototype items into distinct full-map groups without requiring adjacency
};

// The manual-scroll intent a view is currently under. Input decides it, the view consumes it, and the two
// never share a field: a direction is a per-frame intent, not a value anyone configures
///@ ExportEnum
enum class ScrollDirection : uint8_t
{
    None = 0, // No manual-scroll direction is active
    Left = 0x01, // Scroll toward the left edge
    Right = 0x02, // Scroll toward the right edge
    Up = 0x04, // Scroll toward the upper edge
    Down = 0x08, // Scroll toward the lower edge
};

// The layers a map view draws. The mapper hides one to work on another, so the visible set is editor state
// the view is told about - a value that changes while the tool runs is not something anyone configures
///@ ExportEnum
enum class MapLayers : uint8_t
{
    None = 0, // Draw none of the optional map layers
    Items = 0x01, // Draw map items
    Scenery = 0x02, // Draw scenery objects
    Walls = 0x04, // Draw wall objects
    Critters = 0x08, // Draw critters
    Tiles = 0x10, // Draw ground tiles
    Roof = 0x20, // Draw roof tiles
    Fast = 0x40, // Draw the fast-rendered layer
    All = 0x7F, // Draw every map layer
};

class AnimationResolver
{
public:
    [[nodiscard]] virtual auto ResolveCritterAnimationFrames(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& pass, uint32_t& flags, int32_t& ox, int32_t& oy, string& anim_name) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, int32_t& f_state_anim, int32_t& f_action_anim, int32_t& f_state_anim_ex, int32_t& f_action_anim_ex, uint32_t& flags) -> bool = 0;
    virtual ~AnimationResolver() = default;
};

FO_END_NAMESPACE

#endif // FO_PRECOMPILED_HEADER_GUARD
