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

#pragma once

#include "Common.h"

#include "Properties.h"
#include "ScriptSystem.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

///@ ExportEntity Game ServerEngine ClientEngine Global // Global singleton script receiver for the current server, client, or Mapper engine; it exposes engine services, events, and game properties rather than a persistent world object.
///@ ExportEntity Player Player PlayerView HasTimeEvents // Player account and connection-session entity, distinct from the Critter it may control; clients receive its replicated PlayerView.
///@ ExportEntity Location Location LocationView HasProtos HasTimeEvents // Prototype-backed world-location entity that owns instantiated maps on the server and is represented by LocationView on clients.
///@ ExportEntity Map Map MapView HasProtos HasTimeEvents // Prototype-backed instantiated map entity containing runtime critters and items; clients operate on its replicated MapView.
///@ ExportEntity Critter Critter CritterView HasProtos HasTimeEvents // Prototype-backed character entity used for player bodies and NPCs, with inventory, movement, and map or global-map state.
///@ ExportEntity Item Item ItemView HasProtos HasStatics HasAbstract HasTimeEvents // Prototype-backed item entity that can belong to a map, critter inventory, container, or custom holder and also exposes static and abstract variants.

#define FO_ENTITY_PROPERTY(prop_type, prop) \
    inline auto GetProperty##prop() const noexcept -> ptr<const Property> \
    { \
        return _propsRef->GetRegistrar()->GetPropertyByIndexUnsafe(prop##_RegIndex); \
    } \
    inline auto Get##prop() const noexcept \
    { \
        FO_VALIDATE_ENTITY_ACCESS_VALUE(_propsRef->GetEntity()); \
        return _propsRef->GetValueFast<prop_type>(GetProperty##prop()); \
    } \
    inline void Set##prop(const prop_type& value) \
    { \
        FO_VALIDATE_ENTITY_ACCESS_VALUE(_propsRef->GetEntity()); \
        _propsRef->SetValue(GetProperty##prop(), value); \
    } \
    inline bool IsNonEmpty##prop() const noexcept \
    { \
        FO_VALIDATE_ENTITY_ACCESS_VALUE(_propsRef->GetEntity()); \
        return _propsRef->GetRawDataSize(GetProperty##prop()) != 0; \
    } \
    static uint16_t prop##_RegIndex

#define FO_ENTITY_EVENT(event_name, ...) \
    EntityEventWrapper<fixed_string(#event_name) __VA_OPT__(, ) __VA_ARGS__> event_name \
    { \
        ptr<Entity> \
        { \
            this \
        } \
    }

class EntityProperties
{
public:
    // For a custom entity, stores the persistent identifier of its owning holder; zero when the holder has no persistent identifier.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, CustomHolderId);
    // For a custom entity, names the holder entry through which it is attached.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(hstring, CustomHolderEntry);
    // Marks an entity as independently persistent rather than persistent only through containment by another entity.
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(bool, ExplicitlyPersistent);

    explicit EntityProperties(Properties& props) noexcept;

protected:
    ptr<Properties> _propsRef;
};

class TimeEventContext;

enum class EntityHolderEntrySync : uint8_t
{
    NoSync,
    OwnerSync,
    PublicSync,
};

struct EntityEventDesc
{
    string Name {};
    vector<ArgDesc> Args {};
    bool Exported {};
};

struct EntityTypeDesc
{
    struct HolderEntryDesc
    {
        hstring TargetType {};
        EntityHolderEntrySync Sync {};
        bool Persistent {};
    };

    bool Exported {};
    bool IsGlobal {};
    bool HasProtos {};
    bool HasStatics {};
    bool HasAbstract {};
    unique_ptr<PropertyRegistrar> PropRegistrar;
    unordered_map<hstring, HolderEntryDesc> HolderEntries {};
    vector<MethodDesc> Methods {};
    vector<EntityEventDesc> Events {};
};

class Entity
{
    friend class EntityEvent;

public:
    using InnerEntityMap = map<hstring, vector<refcount_ptr<Entity>>>;

    // Controls whether an entity event callback allows lower-priority callbacks to run.
    ///@ ExportEnum
    enum class EventResult : int32_t
    {
        ContinueChain,
        StopChain,
    };
    ///@ EnumValueDoc EventResult ContinueChain // Continues dispatching callbacks with lower priority.
    ///@ EnumValueDoc EventResult StopChain // Stops the current event callback chain immediately.

    using EventCallback = copyable_function<EventResult(FuncCallData&)>;

    // Relative ordering assigned to callbacks subscribed to the same entity event.
    ///@ ExportEnum
    enum class EventPriority : int32_t
    {
        Lowest = 0,
        Low = 1000000,
        Normal = 2000000,
        High = 3000000,
        Highest = 4000000,
    };
    ///@ EnumValueDoc EventPriority Lowest // Runs after callbacks with higher numeric priority; only one callback may occupy the lowest band.
    ///@ EnumValueDoc EventPriority Low // Runs after normal-priority callbacks and before the lowest callback.
    ///@ EnumValueDoc EventPriority Normal // Default callback priority between the low and high bands.
    ///@ EnumValueDoc EventPriority High // Runs after the highest callback and before normal-priority callbacks.
    ///@ EnumValueDoc EventPriority Highest // Runs before callbacks with lower numeric priority; only one callback may occupy the highest band.

    struct EventCallbackData
    {
        EventCallback Callback {};
        uintptr_t SubscriptionPtr {};
        EventPriority Priority {EventPriority::Normal};
        bool HasExplicitResult {};
    };

    struct TimeEventData
    {
        using FuncType = variant<ScriptFunc<void>, ScriptFunc<void, any_t>, ScriptFunc<void, vector<any_t>>, // All possible variants for time events
            ScriptFunc<void, ptr<TimeEventContext>>, ScriptFunc<void, ptr<ScriptSelfEntity>>, ScriptFunc<void, ptr<ScriptSelfEntity>, any_t>, ScriptFunc<void, ptr<ScriptSelfEntity>, vector<any_t>>, ScriptFunc<void, ptr<ScriptSelfEntity>, ptr<TimeEventContext>>>;

        uint32_t Id {};
        FuncType Func {};
        ScriptFuncName FuncName {};
        nanotime FireTime {};
        timespan RepeatDuration {};
        vector<any_t> Data {};
    };

    using TimeEventList = vector<shared_ptr<TimeEventData>>;

    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] virtual auto GetName() const noexcept -> string_view = 0;
    [[nodiscard]] virtual auto GetId() const noexcept -> ident_t { return {}; }
    [[nodiscard]] virtual auto IsGlobal() const noexcept -> bool { return false; }
    [[nodiscard]] auto GetTypeName() const noexcept -> hstring { return _props.GetRegistrar()->GetTypeName(); }
    [[nodiscard]] auto GetTypeNamePlural() const noexcept -> hstring { return _props.GetRegistrar()->GetTypeNamePlural(); }
    [[nodiscard]] auto GetProperties() const noexcept -> ptr<const Properties> { return &_props; }
    [[nodiscard]] auto GetPropertiesForEdit() noexcept -> ptr<Properties> { return &_props; }
    [[nodiscard]] auto IsDestroying() const noexcept -> bool { return _isDestroying.load(std::memory_order_acquire); }
    [[nodiscard]] auto IsDestroyed() const noexcept -> bool { return _isDestroyed.load(std::memory_order_acquire); }
    [[nodiscard]] auto GetValueAsInt(ptr<const Property> prop) const -> int32_t;
    [[nodiscard]] auto GetValueAsInt(int32_t prop_index) const -> int32_t;
    [[nodiscard]] auto GetValueAsAny(ptr<const Property> prop) const -> any_t;
    [[nodiscard]] auto GetValueAsAny(int32_t prop_index) const -> any_t;
    [[nodiscard]] auto HasInnerEntities() const noexcept -> bool { return _innerEntities && !_innerEntities->empty(); }
    [[nodiscard]] auto GetInnerEntitiesCount() const noexcept -> size_t;
    [[nodiscard]] auto GetInnerEntities() const noexcept -> nptr<const InnerEntityMap> { return _innerEntities ? make_nptr(&*_innerEntities) : nullptr; }
    [[nodiscard]] auto GetInnerEntities() noexcept -> nptr<InnerEntityMap> { return _innerEntities ? make_nptr(&*_innerEntities) : nullptr; }
    [[nodiscard]] auto GetInnerEntities(hstring entry) const noexcept -> nptr<const vector<refcount_ptr<Entity>>>;
    [[nodiscard]] auto GetInnerEntities(hstring entry) noexcept -> nptr<vector<refcount_ptr<Entity>>>;
    [[nodiscard]] auto HasEventCallbacks(string_view event_name) const noexcept -> bool;
    [[nodiscard]] auto GetTimeEvents() const noexcept -> nptr<const TimeEventList> { return _timeEvents ? make_nptr(&*_timeEvents) : nullptr; }
    [[nodiscard]] auto GetTimeEvents() noexcept -> nptr<TimeEventList> { return _timeEvents ? make_nptr(&*_timeEvents) : nullptr; }
    [[nodiscard]] auto HasTimeEvents() const noexcept -> bool;

    auto StoreData(bool with_protected) const -> Properties::StoredData;
    void RestoreData(const vector<vector<uint8_t>>& props_data);
    void SetValueFromData(ptr<const Property> prop, PropertyRawData& prop_data);
    void SetValueAsInt(ptr<const Property> prop, int32_t value);
    void SetValueAsInt(int32_t prop_index, int32_t value);
    void SetValueAsAny(ptr<const Property> prop, const any_t& value);
    void SetValueAsAny(int32_t prop_index, const any_t& value);
    auto EnsureTimeEvents() -> ptr<TimeEventList>;
    void SubscribeEvent(string_view event_name, EventCallbackData&& callback);
    void UnsubscribeEvent(string_view event_name, uintptr_t subscription_ptr) noexcept;
    void UnsubscribeAllEvent(string_view event_name) noexcept;
    void UnsubscribeAllEvents() noexcept;
    void ClearAllTimeEvents() noexcept;
    auto FireEvent(string_view event_name, FuncCallData& call) noexcept -> EventResult;
    void AddInnerEntity(hstring entry, ptr<Entity> entity);
    void RemoveInnerEntity(hstring entry, ptr<Entity> entity);
    void ClearInnerEntities();

    void AddRef() const noexcept;
    auto TryAddRef() const noexcept -> bool;
    void Release() const noexcept;
    auto GetRefCount() const noexcept -> int32_t { return _refCounter.load(); }

    virtual void ValidateAccess() const { }
    virtual void LockForPropertyAccess() noexcept { }
    virtual void UnlockForPropertyAccess() noexcept { }
    virtual void LockForPropertyAccessShared() noexcept { LockForPropertyAccess(); }
    virtual void UnlockForPropertyAccessShared() noexcept { UnlockForPropertyAccess(); }

    void MarkAsDestroying() noexcept;
    void MarkAsDestroyed() noexcept;

protected:
    Entity(ptr<const PropertyRegistrar> registrar, nptr<const Properties> init_props, nptr<const Properties> base_props) noexcept;
    virtual ~Entity();

    auto GetInitRef() noexcept -> ptr<Properties> { return &_props; }

protected:
    virtual auto FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult;

private:
    auto FindEventCallbacks(string_view event_name) noexcept -> nptr<vector<EventCallbackData>>;
    auto EnsureEventCallbacks(string_view event_name) -> ptr<vector<EventCallbackData>>;
    void SubscribeEvent(ptr<vector<EventCallbackData>> callbacks, EventCallbackData&& callback);
    void UnsubscribeEvent(ptr<vector<EventCallbackData>> callbacks, uintptr_t subscription_ptr) noexcept;

    Properties _props;
    optional<map<string, vector<EventCallbackData>>> _events {};
    optional<TimeEventList> _timeEvents {};
    optional<InnerEntityMap> _innerEntities {};
    std::atomic_bool _isDestroying {};
    std::atomic_bool _isDestroyed {};
    mutable std::atomic_int _refCounter {1};
};

class EntityEvent
{
public:
    EntityEvent() = delete;
    EntityEvent(const EntityEvent&) = delete;
    EntityEvent(EntityEvent&&) noexcept = delete;
    auto operator=(const EntityEvent&) = delete;
    auto operator=(EntityEvent&&) noexcept = delete;

    void Subscribe(Entity::EventCallbackData&& callback);
    void Unsubscribe(uintptr_t subscription_ptr) noexcept;
    void UnsubscribeAll() noexcept;

protected:
    EntityEvent(ptr<Entity> entity, string_view callback_name) noexcept;
    auto FireEvent(FuncCallData& call) noexcept -> Entity::EventResult;
    auto CheckCallbacks() -> bool;

    ptr<Entity> _entity;
    string_view _callbackName;
    std::atomic<vector<Entity::EventCallbackData>*> _callbacks {};
};

template<fixed_string Name, typename... Args>
class EntityEventWrapper final : public EntityEvent
{
public:
    explicit EntityEventWrapper(ptr<Entity> entity) noexcept :
        EntityEvent(entity, Name.c_str())
    {
    }
    EntityEventWrapper(const EntityEventWrapper&) = delete;
    EntityEventWrapper(EntityEventWrapper&&) noexcept = delete;
    auto operator=(const EntityEventWrapper&) = delete;
    auto operator=(EntityEventWrapper&&) noexcept = delete;

    auto Fire(Args... args) noexcept -> Entity::EventResult
    {
        FO_STACK_TRACE_ENTRY_NAMED(Name.c_str());

        if (!CheckCallbacks()) {
            return Entity::EventResult::ContinueChain;
        }

        if (_entity->IsGlobal()) {
            array<NativeDataProvider::StorageEntryType, sizeof...(Args)> temp_storage {};
            size_t storage_index = 0;
            array<ptr<void>, sizeof...(Args)> args_data {([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};

            auto accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR);
            FuncCallData call {.Accessor = accessor};
            call.ArgsData = args_data;
            return FireEvent(call);
        }
        else {
            array<NativeDataProvider::StorageEntryType, sizeof...(Args) + 1> temp_storage {};
            size_t storage_index = 0;
            nptr<Entity> first_arg = _entity;
            array<ptr<void>, sizeof...(Args) + 1> args_data {make_ptr(first_arg.get_pp()).void_cast(), ([&] { return NativeDataProvider::NormalizeArg(args, temp_storage[storage_index++]); }())...};

            auto accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR);
            FuncCallData call {.Accessor = accessor};
            call.ArgsData = args_data;
            return FireEvent(call);
        }
    }
};

class EntityManagerApi
{
public:
    virtual auto CreateCustomInnerEntity(ptr<Entity> holder, hstring entry, hstring pid) -> nptr<Entity> = 0;
    virtual auto CreateCustomEntity(hstring type_name, hstring pid) -> nptr<Entity> = 0;
    virtual auto GetCustomEntity(hstring type_name, ident_t id) -> refcount_nptr<Entity> = 0;
    virtual void DestroyEntity(ptr<Entity> entity) = 0;
    virtual ~EntityManagerApi() = default;
};

// Null-tolerant convenience wrapper around `Entity::ValidateAccess()`
inline void ValidateEntityAccess(nptr<const Entity> entity)
{
    if (entity) {
        entity->ValidateAccess();
    }
}

FO_END_NAMESPACE
