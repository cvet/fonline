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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "MsgFiles.h"
#include "Properties.h"

///@ ExportEntity Game FOServer FOClient Global
///@ ExportEntity Player Player PlayerView
///@ ExportEntity Item Item ItemView HasProto HasStatics HasAbstract
///@ ExportEntity Critter Critter CritterView HasProto
///@ ExportEntity Map Map MapView HasProto
///@ ExportEntity Location Location LocationView HasProto

static_assert(sizeof(ident_t) == sizeof(uint));
///@ ExportType ident_t uint RelaxedStrong

#define ENTITY_PROPERTY(access_type, prop_type, prop) \
    inline auto GetProperty##prop() const->const Property* \
    { \
        return _propsRef.GetRegistrator()->GetByIndexFast(prop##_RegIndex); \
    } \
    inline prop_type Get##prop() const \
    { \
        return _propsRef.GetValue<prop_type>(GetProperty##prop()); \
    } \
    inline void Set##prop(prop_type value) \
    { \
        _propsRef.SetValue(GetProperty##prop(), value); \
    } \
    inline bool IsNonEmpty##prop() const \
    { \
        return _propsRef.GetRawDataSize(GetProperty##prop()) > 0u; \
    } \
    static uint16 prop##_RegIndex

#define ENTITY_EVENT(event_name, ...) \
    EntityEvent<__VA_ARGS__> event_name \
    { \
        this, #event_name \
    }

class EntityProperties
{
protected:
    explicit EntityProperties(Properties& props);

    Properties& _propsRef;
};

class Entity
{
    friend class EntityEventBase;

public:
    using EventCallback = std::function<bool(const initializer_list<void*>&)>;

    ///@ ExportEnum
    enum class EventExceptionPolicy
    {
        IgnoreAndContinueChain,
        StopChainAndReturnTrue,
        StopChainAndReturnFalse,
        PropogateException,
    };

    ///@ ExportEnum
    enum class EventPriority
    {
        Lowest,
        Low,
        Normal,
        High,
        Highest,
    };

    struct EventCallbackData
    {
        EventCallback Callback {};
        const void* SubscribtionPtr {};
        EventExceptionPolicy ExPolicy {EventExceptionPolicy::IgnoreAndContinueChain}; // Todo: improve entity event ExPolicy
        EventPriority Priority {EventPriority::Normal}; // Todo: improve entity event Priority
        bool OneShot {}; // Todo: improve entity event OneShot
        bool Deferred {}; // Todo: improve entity event Deferred
    };

    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] virtual auto GetName() const -> string_view = 0;
    [[nodiscard]] virtual auto IsGlobal() const -> bool { return false; }
    [[nodiscard]] auto GetClassName() const -> string_view;
    [[nodiscard]] auto GetProperties() const -> const Properties&;
    [[nodiscard]] auto GetPropertiesForEdit() -> Properties&;
    [[nodiscard]] auto IsDestroying() const -> bool;
    [[nodiscard]] auto IsDestroyed() const -> bool;
    [[nodiscard]] auto GetValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetValueAsInt(int prop_index) const -> int;
    [[nodiscard]] auto GetValueAsFloat(const Property* prop) const -> float;
    [[nodiscard]] auto GetValueAsFloat(int prop_index) const -> float;

    void SetProperties(const Properties& props);
    void StoreData(bool with_protected, vector<uint8*>** all_data, vector<uint>** all_data_sizes) const;
    void RestoreData(const vector<const uint8*>& all_data, const vector<uint>& all_data_sizes);
    void RestoreData(const vector<vector<uint8>>& properties_data);
    auto LoadFromText(const map<string, string>& key_values) -> bool;
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetValueAsInt(const Property* prop, int value);
    void SetValueAsInt(int prop_index, int value);
    void SetValueAsFloat(const Property* prop, float value);
    void SetValueAsFloat(int prop_index, float value);
    void SubscribeEvent(const string& event_name, EventCallbackData callback);
    void UnsubscribeEvent(const string& event_name, const void* subscription_ptr);
    void UnsubscribeAllEvent(const string& event_name);
    auto FireEvent(const string& event_name, const initializer_list<void*>& args) -> bool;

    void AddRef() const noexcept;
    void Release() const noexcept;

    void MarkAsDestroying();
    virtual void MarkAsDestroyed();

protected:
    explicit Entity(const PropertyRegistrator* registrator);
    virtual ~Entity() = default;

    auto GetInitRef() -> Properties& { return _props; }

    bool _nonConstHelper {};

private:
    auto GetEventCallbacks(const string& event_name) -> vector<EventCallbackData>*;
    void SubscribeEvent(vector<EventCallbackData>* callbacks, EventCallbackData callback);
    void UnsubscribeEvent(vector<EventCallbackData>* callbacks, const void* subscription_ptr);
    auto FireEvent(vector<EventCallbackData>* callbacks, const initializer_list<void*>& args) -> bool;

    Properties _props;
    map<string, vector<EventCallbackData>> _events;
    bool _isDestroying {};
    bool _isDestroyed {};
    mutable int _refCounter {1};
};

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetName() const -> string_view override;
    [[nodiscard]] auto GetProtoId() const -> hstring;
    [[nodiscard]] auto HasComponent(hstring name) const -> bool;
    [[nodiscard]] auto HasComponent(hstring::hash_t hash) const -> bool;
    [[nodiscard]] auto GetComponents() const -> unordered_set<hstring> { return _components; }

    void EnableComponent(hstring component);

    vector<uint> TextsLang {};
    vector<FOMsg*> Texts {};
    string CollectionName {};

protected:
    ProtoEntity(hstring proto_id, const PropertyRegistrator* registrator);

    const hstring _protoId;
    unordered_set<hstring> _components {};
    unordered_set<hstring::hash_t> _componentHashes {};
};

class EntityWithProto
{
public:
    EntityWithProto() = delete;
    EntityWithProto(const EntityWithProto&) = delete;
    EntityWithProto(EntityWithProto&&) noexcept = delete;
    auto operator=(const EntityWithProto&) = delete;
    auto operator=(EntityWithProto&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const -> hstring;
    [[nodiscard]] auto GetProto() const -> const ProtoEntity*;

protected:
    EntityWithProto(Entity* owner, const ProtoEntity* proto);
    virtual ~EntityWithProto();

    const ProtoEntity* _proto;
};

class EntityEventBase
{
public:
    void Subscribe(Entity::EventCallbackData callback);
    void Unsubscribe(const void* subscription_ptr);
    void UnsubscribeAll();

protected:
    EntityEventBase(Entity* entity, const char* callback_name);

    [[nodiscard]] auto FireEx(const initializer_list<void*>& args_list) const -> bool { return _entity->FireEvent(_callbacks, args_list); }

    Entity* _entity;
    const char* _callbackName;
    vector<Entity::EventCallbackData>* _callbacks {};
};

template<typename... Args>
class EntityEvent final : public EntityEventBase
{
public:
    EntityEvent(Entity* entity, const char* callback_name) :
        EntityEventBase(entity, callback_name)
    {
    }

    auto Fire(Args... args) -> bool
    {
        if (_callbacks == nullptr) {
            return true;
        }

        const initializer_list<void*> args_list = {&args...};
        return FireEx(args_list);
    }
};
