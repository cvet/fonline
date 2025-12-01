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

#pragma once

#include "Common.h"

#include "Properties.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE();

///@ ExportEntity Game FOServer FOClient Global
///@ ExportEntity Player Player PlayerView HasTimeEvents
///@ ExportEntity Location Location LocationView HasProtos HasTimeEvents
///@ ExportEntity Map Map MapView HasProtos HasTimeEvents
///@ ExportEntity Critter Critter CritterView HasProtos HasTimeEvents
///@ ExportEntity Item Item ItemView HasProtos HasStatics HasAbstract HasTimeEvents

#define FO_ENTITY_PROPERTY(prop_type, prop) \
    inline auto GetProperty##prop() const noexcept -> const Property* \
    { \
        return _propsRef->GetRegistrator()->GetPropertyByIndexUnsafe(prop##_RegIndex); \
    } \
    inline auto Get##prop() const noexcept \
    { \
        return _propsRef->GetValueFast<prop_type>(GetProperty##prop()); \
    } \
    inline void Set##prop(const prop_type& value) \
    { \
        _propsRef->SetValue(GetProperty##prop(), value); \
    } \
    inline bool IsNonEmpty##prop() const noexcept \
    { \
        return _propsRef->GetRawDataSize(GetProperty##prop()) != 0; \
    } \
    static uint16 prop##_RegIndex

#define FO_ENTITY_EVENT(event_name, ...) \
    EntityEvent<__VA_ARGS__> event_name \
    { \
        this, #event_name \
    }

class EntityProperties
{
public:
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(ident_t, CustomHolderId);
    ///@ ExportProperty Common Persistent
    FO_ENTITY_PROPERTY(hstring, CustomHolderEntry);

    explicit EntityProperties(Properties& props) noexcept;

protected:
    raw_ptr<Properties> _propsRef;
};

enum class EntityHolderEntryAccess : uint8
{
    Private,
    Protected,
    Public,
};

struct EntityTypeInfo
{
    raw_ptr<const PropertyRegistrator> PropRegistrator {};
    bool Exported {};
    bool HasProtos {};
    unordered_map<hstring, tuple<hstring, EntityHolderEntryAccess>> HolderEntries {}; // Target type, access
};

class Entity
{
    friend class EntityEventBase;

public:
    using EventCallback = function<bool(const initializer_list<void*>&)>;

    ///@ ExportEnum
    enum class EventExceptionPolicy : uint8
    {
        IgnoreAndContinueChain,
        StopChainAndReturnTrue,
        StopChainAndReturnFalse,
    };

    ///@ ExportEnum
    enum class EventPriority : int32
    {
        Lowest = 0,
        Low = 1000000,
        Normal = 2000000,
        High = 3000000,
        Highest = 4000000,
    };

    struct EventCallbackData
    {
        EventCallback Callback {};
        raw_ptr<const void> SubscribtionPtr {};
        EventExceptionPolicy ExPolicy {EventExceptionPolicy::IgnoreAndContinueChain}; // Todo: improve entity event ExPolicy
        EventPriority Priority {EventPriority::Normal};
        bool OneShot {}; // Todo: improve entity event OneShot
        bool Deferred {}; // Todo: improve entity event Deferred
    };

    struct TimeEventData
    {
        uint32 Id {};
        hstring FuncName {};
        nanotime FireTime {};
        timespan RepeatDuration {};
        vector<any_t> Data {};
    };

    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] virtual auto GetName() const noexcept -> string_view = 0;
    [[nodiscard]] virtual auto IsGlobal() const noexcept -> bool { return false; }
    [[nodiscard]] auto GetTypeName() const noexcept -> hstring { return _props.GetRegistrator()->GetTypeName(); }
    [[nodiscard]] auto GetTypeNamePlural() const noexcept -> hstring { return _props.GetRegistrator()->GetTypeNamePlural(); }
    [[nodiscard]] auto GetProperties() const noexcept -> const Properties& { return _props; }
    [[nodiscard]] auto GetPropertiesForEdit() noexcept -> Properties& { return _props; }
    [[nodiscard]] auto IsDestroying() const noexcept -> bool { return _isDestroying; }
    [[nodiscard]] auto IsDestroyed() const noexcept -> bool { return _isDestroyed; }
    [[nodiscard]] auto GetValueAsInt(const Property* prop) const -> int32;
    [[nodiscard]] auto GetValueAsInt(int32 prop_index) const -> int32;
    [[nodiscard]] auto GetValueAsAny(const Property* prop) const -> any_t;
    [[nodiscard]] auto GetValueAsAny(int32 prop_index) const -> any_t;
    [[nodiscard]] auto HasInnerEntities() const noexcept -> bool { return _innerEntities && !_innerEntities->empty(); }
    [[nodiscard]] auto GetInnerEntities() const noexcept -> const auto& { return *_innerEntities; }
    [[nodiscard]] auto GetInnerEntities() noexcept -> auto& { return *_innerEntities; }
    [[nodiscard]] auto GetInnerEntities(hstring entry) const noexcept -> const vector<refcount_ptr<Entity>>*;
    [[nodiscard]] auto GetInnerEntities(hstring entry) noexcept -> vector<refcount_ptr<Entity>>*;
    [[nodiscard]] auto GetRawTimeEvents() noexcept -> auto& { return _timeEvents; }
    [[nodiscard]] auto HasTimeEvents() const noexcept -> bool;

    void StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const;
    void RestoreData(const vector<vector<uint8>>& props_data);
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetValueAsInt(const Property* prop, int32 value);
    void SetValueAsInt(int32 prop_index, int32 value);
    void SetValueAsAny(const Property* prop, const any_t& value);
    void SetValueAsAny(int32 prop_index, const any_t& value);
    void SubscribeEvent(string_view event_name, EventCallbackData&& callback);
    void UnsubscribeEvent(string_view event_name, const void* subscription_ptr) noexcept;
    void UnsubscribeAllEvent(string_view event_name) noexcept;
    auto FireEvent(string_view event_name, const initializer_list<void*>& args) noexcept -> bool;
    void AddInnerEntity(hstring entry, Entity* entity);
    void RemoveInnerEntity(hstring entry, Entity* entity);
    void ClearInnerEntities();

    void AddRef() const noexcept;
    void Release() const noexcept;

    void MarkAsDestroying() noexcept;
    void MarkAsDestroyed() noexcept;

protected:
    Entity(const PropertyRegistrator* registrator, const Properties* props) noexcept;
    virtual ~Entity() = default;

    auto GetInitRef() noexcept -> Properties& { return _props; }

    bool _nonConstHelper {};

private:
    auto GetEventCallbacks(string_view event_name) -> vector<EventCallbackData>&;
    void SubscribeEvent(vector<EventCallbackData>& callbacks, EventCallbackData&& callback);
    void UnsubscribeEvent(vector<EventCallbackData>& callbacks, const void* subscription_ptr) noexcept;
    auto FireEvent(vector<EventCallbackData>& callbacks, const initializer_list<void*>& args) noexcept -> bool;

    Properties _props;
    unique_ptr<map<string, vector<EventCallbackData>>> _events {}; // Todo: entity events map key to hstring
    unique_ptr<vector<shared_ptr<TimeEventData>>> _timeEvents {};
    unique_ptr<map<hstring, vector<refcount_ptr<Entity>>>> _innerEntities {};
    bool _isDestroying {};
    bool _isDestroyed {};
    mutable std::atomic_int _refCounter {1};
};

class EntityEventBase
{
public:
    void Subscribe(Entity::EventCallbackData&& callback);
    void Unsubscribe(const void* subscription_ptr) noexcept;
    void UnsubscribeAll() noexcept;

protected:
    EntityEventBase(Entity* entity, const char* callback_name) noexcept;

    [[nodiscard]] auto FireEx(const initializer_list<void*>& args_list) const noexcept -> bool
    {
        FO_STRONG_ASSERT(_callbacks);

        return _entity->FireEvent(*_callbacks, args_list);
    }

    Entity* _entity;
    const char* _callbackName;
    vector<Entity::EventCallbackData>* _callbacks {};
};

template<typename... Args>
class EntityEvent final : public EntityEventBase
{
public:
    EntityEvent(Entity* entity, const char* callback_name) noexcept :
        EntityEventBase(entity, callback_name)
    {
    }

    auto Fire(Args... args) noexcept -> bool
    {
        if (_callbacks == nullptr) {
            return true;
        }

        const initializer_list<void*> args_list = {static_cast<void*>(&args)...};
        return FireEx(args_list);
    }
};

FO_END_NAMESPACE();
