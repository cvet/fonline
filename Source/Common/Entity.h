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

#pragma once

#include "Common.h"

#include "MsgFiles.h"
#include "Properties.h"

///@ ExportEntity Game FOServer FOClient Global
///@ ExportEntity Player Player PlayerView
///@ ExportEntity Item Item ItemView
///@ ExportEntity Critter Critter CritterView
///@ ExportEntity Map Map MapView
///@ ExportEntity Location Location LocationView

#define ENTITY_PROPERTY(access_type, prop_type, prop, prop_index) \
    static constexpr ushort prop##_RegIndex = prop_index; \
    static constexpr Property::AccessType prop##_AccessType = Property::AccessType::access_type; \
    static constexpr const std::type_info& prop##_TypeId = typeid(prop_type); \
    inline auto GetProperty##prop() const->const Property* { return _propsRef.GetRegistrator()->GetByIndex(prop##_RegIndex); } \
    inline prop_type Get##prop() const { return _propsRef.GetValue<prop_type>(GetProperty##prop()); } \
    inline void Set##prop(prop_type value) { _propsRef.SetValue<prop_type>(GetProperty##prop(), value); } \
    inline bool IsNonEmpty##prop() const { return _propsRef.GetRawDataSize(GetProperty##prop()) > 0u; }

#define REGISTER_ENTITY_PROPERTY(prop) \
    const auto* prop_##prop = registrator->Register(prop##_AccessType, prop##_TypeId, #prop); \
    RUNTIME_ASSERT(prop_##prop->GetRegIndex() == prop##_RegIndex)

class EntityProperties
{
protected:
    explicit EntityProperties(Properties& props);

    Properties& _propsRef;
};

class Entity
{
public:
    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] auto GetClassName() const -> string_view;
    [[nodiscard]] auto GetProperties() const -> const Properties&;
    [[nodiscard]] auto GetPropertiesForEdit() -> Properties&;
    [[nodiscard]] auto IsDestroying() const -> bool;
    [[nodiscard]] auto IsDestroyed() const -> bool;
    [[nodiscard]] auto GetValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetValueAsFloat(const Property* prop) const -> float;

    void SetProperties(const Properties& props);
    auto StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint;
    void RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes);
    void RestoreData(const vector<vector<uchar>>& properties_data);
    auto LoadFromText(const map<string, string>& key_values) -> bool;
    void SetValueFromData(const Property* prop, const vector<uchar>& data, bool ignore_send);
    void SetValueAsInt(const Property* prop, int value);
    void SetValueAsFloat(const Property* prop, float value);

    void AddRef() const;
    void Release() const;

    void MarkAsDestroying();
    void MarkAsDestroyed();

protected:
    explicit Entity(const PropertyRegistrator* registrator);
    virtual ~Entity() = default;

    auto GetInitRef() -> Properties& { return _props; }

    bool _nonConstHelper {};

private:
    Properties _props;
    bool _isDestroying {};
    bool _isDestroyed {};
    mutable int _refCounter {1};
};

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;
    [[nodiscard]] auto HaveComponent(hash name) const -> bool;

    const hash ProtoId;
    vector<uint> TextsLang {};
    vector<FOMsg*> Texts {};
    set<hash> Components {};
    string CollectionName {};

protected:
    ProtoEntity(hash proto_id, const PropertyRegistrator* registrator);
};

class EntityWithProto : public Entity
{
public:
    EntityWithProto() = delete;
    EntityWithProto(const EntityWithProto&) = delete;
    EntityWithProto(EntityWithProto&&) noexcept = delete;
    auto operator=(const EntityWithProto&) = delete;
    auto operator=(EntityWithProto&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const -> hash;
    [[nodiscard]] auto GetName() const -> string;

    const ProtoEntity* Proto;

protected:
    EntityWithProto(const PropertyRegistrator* registrator, const ProtoEntity* proto);
    ~EntityWithProto() override;
};
