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

// Todo: don't preserve memory for not allocated components in entity
// Todo: pack bool properties to one bit

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(PropertyRegistrationException);
FO_DECLARE_EXCEPTION(PropertiesException);

class Entity;
class Property;
class PropertyRegistrator;
class Properties;

enum class PropertiesRelationType : uint8
{
    BothRelative,
    ServerRelative,
    ClientRelative,
};

class PropertyRawData
{
public:
    static constexpr size_t LOCAL_BUF_SIZE = 256;

    PropertyRawData() = default;
    PropertyRawData(const PropertyRawData&) = delete;
    PropertyRawData(PropertyRawData&&) noexcept = default;
    auto operator=(const PropertyRawData&) = delete;
    auto operator=(PropertyRawData&& other) noexcept -> PropertyRawData& = default;
    ~PropertyRawData() = default;

    [[nodiscard]] auto GetPtr() noexcept -> void*;
    [[nodiscard]] auto GetSize() const noexcept -> size_t { return _dataSize; }

    template<typename T>
    [[nodiscard]] auto GetPtrAs() noexcept -> T*
    {
        return static_cast<T*>(GetPtr());
    }

    template<typename T>
    [[nodiscard]] auto GetAs() noexcept -> T
    {
        FO_STRONG_ASSERT(sizeof(T) == _dataSize);
        return *static_cast<T*>(GetPtr());
    }

    auto Alloc(size_t size) noexcept -> uint8*;
    void Set(const void* value, size_t size) noexcept { MemCopy(Alloc(size), value, size); }

    template<typename T>
    void SetAs(T value) noexcept
    {
        MemCopy(Alloc(sizeof(T)), &value, sizeof(T));
    }

    void Pass(span<const uint8> value) noexcept;
    void Pass(const void* value, size_t size) noexcept;
    void StoreIfPassed() noexcept;

private:
    size_t _dataSize {};
    bool _useDynamic {};
    uint8 _localBuf[LOCAL_BUF_SIZE] {};
    unique_arr_ptr<uint8> _dynamicBuf {};
    raw_ptr<void> _passedPtr {};
};

using PropertyGetCallback = function<PropertyRawData(Entity*, const Property*)>;
using PropertySetCallback = function<void(Entity*, const Property*, PropertyRawData&)>;
using PropertyPostSetCallback = function<void(Entity*, const Property*)>;

class Property final
{
    friend class PropertyRegistrator;
    friend class Properties;
    friend class SafeAlloc;

public:
    Property(const Property&) = delete;
    Property(Property&&) noexcept = default;
    auto operator=(const Property&) -> Property& = delete;
    auto operator=(Property&&) noexcept -> Property& = default;
    ~Property() = default;

    [[nodiscard]] auto GetRegistrator() const noexcept -> const PropertyRegistrator* { return _registrator.get(); }
    [[nodiscard]] auto GetName() const noexcept -> const string& { return _propName; }
    [[nodiscard]] auto GetNameWithoutComponent() const noexcept -> const string& { return _propNameWithoutComponent; }
    [[nodiscard]] auto GetComponent() const noexcept -> const hstring& { return _component; }
    [[nodiscard]] auto GetRegIndex() const noexcept -> uint16 { return _regIndex; }
    [[nodiscard]] auto GetBaseScriptFuncType() const noexcept -> const string& { return _scriptFuncType; }

    [[nodiscard]] auto GetBaseTypeInfo() const noexcept -> const BaseTypeInfo& { return _baseType; }
    [[nodiscard]] auto GetBaseTypeName() const noexcept -> const string& { return _baseType.TypeName; }
    [[nodiscard]] auto GetBaseSize() const noexcept -> size_t { return _baseType.Size; }
    [[nodiscard]] auto IsBaseTypeSimpleStruct() const noexcept -> bool { return _baseType.IsSimpleStruct; }
    [[nodiscard]] auto IsBaseTypeComplexStruct() const noexcept -> bool { return _baseType.IsComplexStruct; }
    [[nodiscard]] auto IsBaseTypeStruct() const noexcept -> bool { return _baseType.IsSimpleStruct || _baseType.IsComplexStruct; }
    [[nodiscard]] auto GetBaseTypeLayout() const noexcept { return _baseType.StructLayout; }
    [[nodiscard]] auto GetStructFirstType() const noexcept -> const BaseTypeInfo& { return _baseType.StructLayout->front().second; }
    [[nodiscard]] auto IsBaseTypePrimitive() const noexcept -> bool { return _baseType.IsPrimitive; }
    [[nodiscard]] auto IsBaseTypeInt() const noexcept -> bool { return _baseType.IsInt; }
    [[nodiscard]] auto IsBaseTypeSignedInt() const noexcept -> bool { return _baseType.IsSignedInt; }
    [[nodiscard]] auto IsBaseTypeInt8() const noexcept -> bool { return _baseType.IsInt8; }
    [[nodiscard]] auto IsBaseTypeInt16() const noexcept -> bool { return _baseType.IsInt16; }
    [[nodiscard]] auto IsBaseTypeInt32() const noexcept -> bool { return _baseType.IsInt32; }
    [[nodiscard]] auto IsBaseTypeInt64() const noexcept -> bool { return _baseType.IsInt64; }
    [[nodiscard]] auto IsBaseTypeUInt8() const noexcept -> bool { return _baseType.IsUInt8; }
    [[nodiscard]] auto IsBaseTypeUInt16() const noexcept -> bool { return _baseType.IsUInt16; }
    [[nodiscard]] auto IsBaseTypeUInt32() const noexcept -> bool { return _baseType.IsUInt32; }
    [[nodiscard]] auto IsBaseTypeUInt64() const noexcept -> bool { return _baseType.IsUInt64; }
    [[nodiscard]] auto IsBaseTypeFloat() const noexcept -> bool { return _baseType.IsFloat; }
    [[nodiscard]] auto IsBaseTypeSingleFloat() const noexcept -> bool { return _baseType.IsSingleFloat; }
    [[nodiscard]] auto IsBaseTypeDoubleFloat() const noexcept -> bool { return _baseType.IsDoubleFloat; }
    [[nodiscard]] auto IsBaseTypeBool() const noexcept -> bool { return _baseType.IsBool; }
    [[nodiscard]] auto IsBaseTypeHash() const noexcept -> bool { return _baseType.IsHash; }
    [[nodiscard]] auto IsBaseTypeEnum() const noexcept -> bool { return _baseType.IsEnum; }
    [[nodiscard]] auto IsBaseTypeResource() const noexcept -> bool { return _isResourceHash; }
    [[nodiscard]] auto IsBaseTypeString() const noexcept -> bool { return _baseType.IsString; }
    [[nodiscard]] auto IsPlainData() const noexcept -> bool { return _isPlainData; }
    [[nodiscard]] auto IsString() const noexcept -> bool { return _isString; }
    [[nodiscard]] auto IsArray() const noexcept -> bool { return _isArray; }
    [[nodiscard]] auto IsArrayOfString() const noexcept -> bool { return _isArrayOfString; }
    [[nodiscard]] auto IsDict() const noexcept -> bool { return _isDict; }
    [[nodiscard]] auto IsDictOfString() const noexcept -> bool { return _isDictOfString; }
    [[nodiscard]] auto IsDictOfArray() const noexcept -> bool { return _isDictOfArray; }
    [[nodiscard]] auto IsDictOfArrayOfString() const noexcept -> bool { return _isDictOfArrayOfString; }
    [[nodiscard]] auto IsDictKeyHash() const noexcept -> bool { return _isDictKeyHash; }
    [[nodiscard]] auto IsDictKeyEnum() const noexcept -> bool { return _isDictKeyEnum; }
    [[nodiscard]] auto IsDictKeyString() const noexcept -> bool { return _isDictKeyString; }
    [[nodiscard]] auto GetDictKeyTypeInfo() const noexcept -> const BaseTypeInfo& { return _dictKeyType; }
    [[nodiscard]] auto GetDictKeySize() const noexcept -> size_t { return _dictKeyType.Size; }
    [[nodiscard]] auto GetDictKeyTypeName() const noexcept -> const string& { return _dictKeyType.TypeName; }
    [[nodiscard]] auto GetViewTypeName() const noexcept -> const string& { return _viewTypeName; }

    [[nodiscard]] auto IsDisabled() const noexcept -> bool { return _isDisabled; }
    [[nodiscard]] auto IsCoreProperty() const noexcept -> bool { return _isCoreProperty; }
    [[nodiscard]] auto IsCommon() const noexcept -> bool { return _isCommon; }
    [[nodiscard]] auto IsServerOnly() const noexcept -> bool { return _isServerOnly; }
    [[nodiscard]] auto IsClientOnly() const noexcept -> bool { return _isClientOnly; }
    [[nodiscard]] auto IsSynced() const noexcept -> bool { return _isSynced; }
    [[nodiscard]] auto IsOwnerSync() const noexcept -> bool { return _isOwnerSync; }
    [[nodiscard]] auto IsPublicSync() const noexcept -> bool { return _isPublicSync; }
    [[nodiscard]] auto IsModifiableByClient() const noexcept -> bool { return _isModifiableByClient; }
    [[nodiscard]] auto IsModifiableByAnyClient() const noexcept -> bool { return _isModifiableByAnyClient; }
    [[nodiscard]] auto IsVirtual() const noexcept -> bool { return _isVirtual; }
    [[nodiscard]] auto IsMutable() const noexcept -> bool { return _isMutable; }
    [[nodiscard]] auto IsPersistent() const noexcept -> bool { return _isPersistent; }
    [[nodiscard]] auto IsHistorical() const noexcept -> bool { return _isHistorical; }
    [[nodiscard]] auto IsNullGetterForProto() const noexcept -> bool { return _isNullGetterForProto; }
    [[nodiscard]] auto IsTemporary() const noexcept -> bool { return (_isMutable || _isCoreProperty) && !_isPersistent; }

    [[nodiscard]] auto GetGetter() const noexcept -> auto& { return _getter; }
    [[nodiscard]] auto GetSetters() const noexcept -> auto& { return _setters; }
    [[nodiscard]] auto GetPostSetters() const noexcept -> auto& { return _postSetters; }

    void SetGetter(PropertyGetCallback getter) const;
    void AddSetter(PropertySetCallback setter) const;
    void AddPostSetter(PropertyPostSetCallback setter) const;

private:
    explicit Property(const PropertyRegistrator* registrator);

    raw_ptr<const PropertyRegistrator> _registrator;

    mutable PropertyGetCallback _getter {};
    mutable vector<PropertySetCallback> _setters {};
    mutable vector<PropertyPostSetCallback> _postSetters {};

    string _propName {};
    string _propNameWithoutComponent {};
    hstring _component {};
    string _scriptFuncType {};

    BaseTypeInfo _baseType {};

    bool _isPlainData {};
    bool _isResourceHash {};

    bool _isString {};

    bool _isArray {};
    bool _isArrayOfString {};

    bool _isDict {};
    bool _isDictOfString {};
    bool _isDictOfArray {};
    bool _isDictOfArrayOfString {};
    bool _isDictKeyHash {};
    bool _isDictKeyEnum {};
    bool _isDictKeyString {};
    BaseTypeInfo _dictKeyType {};

    string _viewTypeName {};

    bool _isDisabled {};
    bool _isCoreProperty {};
    bool _isCommon {};
    bool _isServerOnly {};
    bool _isClientOnly {};
    bool _isVirtual {};
    bool _isMutable {};
    bool _isPersistent {};
    bool _isSynced {};
    bool _isOwnerSync {};
    bool _isPublicSync {};
    bool _isNoSync {};
    bool _isModifiableByClient {};
    bool _isModifiableByAnyClient {};
    bool _isHistorical {};
    bool _isNullGetterForProto {};
    uint16 _regIndex {};
    optional<size_t> _podDataOffset {};
    optional<size_t> _complexDataIndex {};
};

class Properties final
{
    friend class PropertyRegistrator;
    friend class Property;

public:
    Properties() = delete;
    explicit Properties(const PropertyRegistrator* registrator) noexcept;
    Properties(const Properties& other) = delete;
    Properties(Properties&&) noexcept = default;
    auto operator=(const Properties& other) noexcept = delete;
    auto operator=(Properties&&) noexcept = delete;
    ~Properties() = default;

    [[nodiscard]] auto GetRegistrator() const noexcept -> const PropertyRegistrator* { return _registrator.get(); }
    [[nodiscard]] auto GetEntity() const noexcept -> Entity* { return _entity.get(); }
    [[nodiscard]] auto Copy() const noexcept -> Properties;
    [[nodiscard]] auto GetRawData(const Property* prop) const noexcept -> span<const uint8>;
    [[nodiscard]] auto GetRawDataSize(const Property* prop) const noexcept -> size_t;
    [[nodiscard]] auto GetPlainDataValueAsInt(const Property* prop) const -> int32;
    [[nodiscard]] auto GetPlainDataValueAsAny(const Property* prop) const -> any_t;
    [[nodiscard]] auto GetValueAsInt(int32 property_index) const -> int32;
    [[nodiscard]] auto GetValueAsAny(int32 property_index) const -> any_t;
    [[nodiscard]] auto SavePropertyToText(const Property* prop) const -> string;
    [[nodiscard]] auto SaveToText(const Properties* base) const -> map<string, string>;
    [[nodiscard]] auto CompareData(const Properties& other, span<const Property*> ignore_props, bool ignore_temporary) const -> bool;

    void AllocData() noexcept;
    void SetEntity(Entity* entity) const noexcept { _entity = entity; }
    void CopyFrom(const Properties& other) noexcept;
    void ValidateForRawData(const Property* prop) const noexcept(false);
    void ApplyFromText(const map<string, string>& key_values);
    void ApplyPropertyFromText(const Property* prop, string_view text);
    void StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const;
    void RestoreAllData(const vector<uint8>& all_data);
    void StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const;
    void RestoreData(const vector<const uint8*>& all_data, const vector<uint32>& all_data_sizes);
    void RestoreData(const vector<vector<uint8>>& all_data);
    void SetRawData(const Property* prop, span<const uint8> raw_data) noexcept;
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetPlainDataValueAsInt(const Property* prop, int32 value);
    void SetPlainDataValueAsAny(const Property* prop, const any_t& value);
    void SetValueAsInt(int32 property_index, int32 value);
    void SetValueAsAny(int32 property_index, const any_t& value);
    void SetValueAsIntProps(int32 property_index, int32 value);
    void SetValueAsAnyProps(int32 property_index, const any_t& value);
    auto ResolveHash(hstring::hash_t h) const -> hstring;
    auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T;
    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T;
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T;
    template<typename T>
        requires(is_vector_collection<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T;
    template<typename T>
        requires(is_map_collection<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T = delete;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T;
    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T;
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> string_view;
    template<typename T>
        requires(is_vector_collection<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T;
    template<typename T>
        requires(is_map_collection<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const -> T = delete;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
    void SetValue(const Property* prop, T new_value);
    template<typename T>
        requires(std::same_as<T, hstring>)
    void SetValue(const Property* prop, T new_value);
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    void SetValue(const Property* prop, const T& new_value);
    template<typename T>
    void SetValue(const Property* prop, const vector<T>& new_value);
    template<typename T, typename U>
    void SetValue(const Property* prop, const map<T, U>& new_value) = delete;

    void SetValue(const Property* prop, PropertyRawData& prop_data);

private:
    raw_ptr<const PropertyRegistrator> _registrator;
    unique_arr_ptr<uint8> _podData {};
    unique_arr_ptr<pair<unique_arr_ptr<uint8>, size_t>> _complexData {};

    mutable unique_ptr<vector<const uint8*>> _storeData {};
    mutable unique_ptr<vector<uint32>> _storeDataSizes {};
    mutable unique_ptr<vector<uint16>> _storeDataComplexIndices {};
    mutable raw_ptr<Entity> _entity {};
};

class PropertyRegistrator final
{
    friend class Properties;
    friend class Property;

public:
    PropertyRegistrator() = delete;
    PropertyRegistrator(string_view type_name, PropertiesRelationType relation, HashResolver& hash_resolver, NameResolver& name_resolver);
    PropertyRegistrator(const PropertyRegistrator&) = delete;
    PropertyRegistrator(PropertyRegistrator&&) noexcept = default;
    auto operator=(const PropertyRegistrator&) = delete;
    auto operator=(PropertyRegistrator&&) noexcept = delete;
    ~PropertyRegistrator();

    [[nodiscard]] auto GetTypeName() const noexcept -> hstring { return _typeName; }
    [[nodiscard]] auto GetTypeNamePlural() const noexcept -> hstring { return _typeNamePlural; }
    [[nodiscard]] auto GetRelation() const noexcept -> PropertiesRelationType { return _relation; }
    [[nodiscard]] auto GetPropertiesCount() const noexcept -> size_t { return _registeredProperties.size(); }
    [[nodiscard]] auto FindProperty(string_view property_name, bool* is_component = nullptr) const -> const Property*;
    [[nodiscard]] auto GetPropertyByIndex(int32 property_index) const noexcept -> const Property*;
    [[nodiscard]] auto GetPropertyByIndexUnsafe(size_t property_index) const noexcept -> const Property* { return _registeredProperties[property_index].get(); }
    [[nodiscard]] auto IsComponentRegistered(hstring component_name) const noexcept -> bool;
    [[nodiscard]] auto GetWholeDataSize() const noexcept -> size_t { return _wholePodDataSize; }
    [[nodiscard]] auto GetProperties() const noexcept -> const vector<raw_ptr<const Property>>& { return _constRegisteredProperties; }
    [[nodiscard]] auto GetPropertyGroups() const noexcept -> map<string, vector<const Property*>>;
    [[nodiscard]] auto GetComponents() const noexcept -> const unordered_set<hstring>& { return _registeredComponents; }
    [[nodiscard]] auto GetHashResolver() const noexcept -> HashResolver* { return _hashResolver.get(); }
    [[nodiscard]] auto GetNameResolver() const noexcept -> NameResolver* { return _nameResolver.get(); }

    void RegisterComponent(string_view name);
    void RegisterProperty(const initializer_list<string_view>& flags) { RegisterProperty({flags.begin(), flags.end()}); }
    void RegisterProperty(const span<const string_view>& flags);

private:
    const hstring _typeName;
    const hstring _typeNamePlural;
    const PropertiesRelationType _relation;
    const hstring _propMigrationRuleName;
    const hstring _componentMigrationRuleName;
    mutable raw_ptr<HashResolver> _hashResolver;
    mutable raw_ptr<NameResolver> _nameResolver;
    vector<unique_ptr<Property>> _registeredProperties {};
    vector<raw_ptr<const Property>> _constRegisteredProperties {};
    unordered_map<string, raw_ptr<const Property>> _registeredPropertiesLookup {};
    unordered_set<hstring> _registeredComponents {};
    map<string, vector<pair<raw_ptr<const Property>, int32>>> _propertyGroups {};

    // PlainData info
    size_t _wholePodDataSize {};
    vector<bool> _publicPodDataSpace {};
    vector<bool> _protectedPodDataSpace {};
    vector<bool> _privatePodDataSpace {};

    // Complex types info
    vector<raw_ptr<Property>> _complexProperties {};
    vector<uint16> _publicComplexDataProps {};
    vector<uint16> _protectedComplexDataProps {};
    vector<uint16> _publicProtectedComplexDataProps {};
};

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
auto Properties::GetValue(const Property* prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));
    FO_RUNTIME_ASSERT(prop->IsPlainData());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity.get(), prop);
            T result = prop_data.GetAs<T>();
            return result;
        }
        else {
            return {};
        }
    }

    FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value());
    auto result = *reinterpret_cast<const T*>(&_podData[*prop->_podDataOffset]);
    return result;
}

template<typename T>
    requires(std::same_as<T, hstring>)
auto Properties::GetValue(const Property* prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
    FO_RUNTIME_ASSERT(prop->IsPlainData());
    FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity.get(), prop);
            const auto hash = prop_data.GetAs<hstring::hash_t>();
            auto result = ResolveHash(hash);
            return result;
        }
        else {
            return {};
        }
    }

    FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value());
    const auto hash = *reinterpret_cast<const hstring::hash_t*>(&_podData[*prop->_podDataOffset]);
    auto result = ResolveHash(hash);
    return result;
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
auto Properties::GetValue(const Property* prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->IsString());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity.get(), prop);
            auto result = string(prop_data.GetPtrAs<char>(), prop_data.GetSize());
            return result;
        }
        else {
            return {};
        }
    }

    FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
    const auto& complex_data = _complexData[*prop->_complexDataIndex];
    auto result = string(reinterpret_cast<const char*>(complex_data.first.get()), complex_data.second);
    return result;
}

template<typename T>
    requires(is_vector_collection<T>)
auto Properties::GetValue(const Property* prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->IsArray());

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);

        if (prop->_getter) {
            prop_data = prop->_getter(_entity.get(), prop);
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());
        const auto& complex_data = _complexData[*prop->_complexDataIndex];
        prop_data.Pass({complex_data.first.get(), complex_data.second});
    }

    const auto* data = prop_data.GetPtrAs<uint8>();
    const auto data_size = prop_data.GetSize();

    T result;

    if (data_size != 0) {
        if constexpr (std::same_as<T, vector<string>> || std::same_as<T, vector<any_t>>) {
            FO_RUNTIME_ASSERT(prop->IsArrayOfString());

            uint32 arr_size;
            MemCopy(&arr_size, data, sizeof(arr_size));
            data += sizeof(arr_size);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for ([[maybe_unused]] const auto i : iterate_range(arr_size)) {
                uint32 str_size;
                MemCopy(&str_size, data, sizeof(str_size));
                data += sizeof(str_size);
                result.emplace_back(string(reinterpret_cast<const char*>(data), str_size));
                data += str_size;
            }
        }
        else if constexpr (std::same_as<T, vector<hstring>>) {
            FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
            FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

            const auto arr_size = data_size / sizeof(hstring::hash_t);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for ([[maybe_unused]] const auto i : iterate_range(arr_size)) {
                const auto hvalue = ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data));
                result.emplace_back(hvalue);
                data += sizeof(hstring::hash_t);
            }
        }
        else {
            FO_RUNTIME_ASSERT(data_size % prop->GetBaseSize() == 0);
            const auto arr_size = data_size / prop->GetBaseSize();
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);
            result.resize(arr_size);
            MemCopy(result.data(), data, data_size);
        }
    }

    return result;
}

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
auto Properties::GetValueFast(const Property* prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled());
    FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(T));
    FO_STRONG_ASSERT(prop->IsPlainData());
    FO_STRONG_ASSERT(!prop->IsVirtual());

    FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
    auto result = *reinterpret_cast<const T*>(&_podData[*prop->_podDataOffset]);
    return result;
}

template<typename T>
    requires(std::same_as<T, hstring>)
auto Properties::GetValueFast(const Property* prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled());
    FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
    FO_STRONG_ASSERT(prop->IsPlainData());
    FO_STRONG_ASSERT(prop->IsBaseTypeHash());
    FO_STRONG_ASSERT(!prop->IsVirtual());

    FO_STRONG_ASSERT(prop->_podDataOffset.has_value());
    const auto hash = *reinterpret_cast<const hstring::hash_t*>(&_podData[*prop->_podDataOffset]);
    auto result = ResolveHash(hash, nullptr);
    return result;
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
auto Properties::GetValueFast(const Property* prop) const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled());
    FO_STRONG_ASSERT(prop->IsString());
    FO_STRONG_ASSERT(!prop->IsVirtual());

    FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
    const auto& complex_data = _complexData[*prop->_complexDataIndex];
    const auto result = string_view(reinterpret_cast<const char*>(complex_data.first.get()), complex_data.second);
    return result;
}

template<typename T>
    requires(is_vector_collection<T>)
auto Properties::GetValueFast(const Property* prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled());
    FO_STRONG_ASSERT(prop->IsArray());
    FO_STRONG_ASSERT(!prop->IsVirtual());

    PropertyRawData prop_data;

    FO_STRONG_ASSERT(prop->_complexDataIndex.has_value());
    const auto& complex_data = _complexData[*prop->_complexDataIndex];
    prop_data.Pass({complex_data.first.get(), complex_data.second});

    const auto* data = prop_data.GetPtrAs<uint8>();
    const auto data_size = prop_data.GetSize();

    T result;

    if (data_size != 0) {
        if constexpr (std::same_as<T, vector<string>> || std::same_as<T, vector<any_t>>) {
            FO_STRONG_ASSERT(prop->IsArrayOfString());

            uint32 arr_size;
            MemCopy(&arr_size, data, sizeof(arr_size));
            data += sizeof(arr_size);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for ([[maybe_unused]] const auto i : iterate_range(arr_size)) {
                uint32 str_size;
                MemCopy(&str_size, data, sizeof(str_size));
                data += sizeof(str_size);
                result.emplace_back(string(reinterpret_cast<const char*>(data), str_size));
                data += str_size;
            }
        }
        else if constexpr (std::same_as<T, vector<hstring>>) {
            FO_STRONG_ASSERT(prop->IsBaseTypeHash());
            FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

            const auto arr_size = data_size / sizeof(hstring::hash_t);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for ([[maybe_unused]] const auto i : iterate_range(arr_size)) {
                const auto hvalue = ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data), nullptr);
                result.emplace_back(hvalue);
                data += sizeof(hstring::hash_t);
            }
        }
        else {
            FO_STRONG_ASSERT(data_size % prop->GetBaseSize() == 0);
            const auto arr_size = data_size / prop->GetBaseSize();
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);
            result.resize(arr_size);
            MemCopy(result.data(), data, data_size);
        }
    }

    return result;
}

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_property_plain_type<T> || is_strong_type<T>)
void Properties::SetValue(const Property* prop, T new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));
    FO_RUNTIME_ASSERT(prop->IsPlainData());
    FO_RUNTIME_ASSERT(prop->IsMutable() || prop->IsCoreProperty());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);
        FO_RUNTIME_ASSERT(!prop->_setters.empty());

        PropertyRawData prop_data;
        prop_data.SetAs<T>(new_value);

        for (const auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value());

        auto& cur_value = *reinterpret_cast<T*>(&_podData[*prop->_podDataOffset]);
        bool equal;

        if constexpr (std::floating_point<T>) {
            equal = is_float_equal(new_value, cur_value);
        }
        else {
            equal = new_value == cur_value;
        }

        if (!equal) {
            if (!prop->_setters.empty() && _entity) {
                PropertyRawData prop_data;
                prop_data.SetAs<T>(new_value);

                for (const auto& setter : prop->_setters) {
                    setter(_entity.get(), prop, prop_data);
                }

                cur_value = prop_data.GetAs<T>();
            }
            else {
                cur_value = new_value;
            }

            if (_entity) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity.get(), prop);
                }
            }
        }
    }
}

template<typename T>
    requires(std::same_as<T, hstring>)
void Properties::SetValue(const Property* prop, T new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
    FO_RUNTIME_ASSERT(prop->IsPlainData());
    FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
    FO_RUNTIME_ASSERT(prop->IsMutable() || prop->IsCoreProperty());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);
        FO_RUNTIME_ASSERT(!prop->_setters.empty());

        PropertyRawData prop_data;
        prop_data.SetAs<hstring::hash_t>(new_value.as_hash());

        for (const auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->_podDataOffset.has_value());
        const auto new_value_hash = new_value.as_hash();

        if (new_value_hash != *reinterpret_cast<hstring::hash_t*>(&_podData[*prop->_podDataOffset])) {
            if (!prop->_setters.empty() && _entity) {
                PropertyRawData prop_data;
                prop_data.SetAs<hstring::hash_t>(new_value_hash);

                for (const auto& setter : prop->_setters) {
                    setter(_entity.get(), prop, prop_data);
                }

                *reinterpret_cast<hstring::hash_t*>(&_podData[*prop->_podDataOffset]) = prop_data.GetAs<hstring::hash_t>();
            }
            else {
                *reinterpret_cast<hstring::hash_t*>(&_podData[*prop->_podDataOffset]) = new_value_hash;
            }

            if (_entity) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity.get(), prop);
                }
            }
        }
    }
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
void Properties::SetValue(const Property* prop, const T& new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->IsString());
    FO_RUNTIME_ASSERT(prop->IsMutable() || prop->IsCoreProperty());

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);
        FO_RUNTIME_ASSERT(!prop->_setters.empty());

        PropertyRawData prop_data;
        prop_data.Pass(new_value.c_str(), new_value.length());

        for (const auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());

        if (!prop->_setters.empty() && _entity) {
            PropertyRawData prop_data;
            prop_data.Pass(new_value.c_str(), new_value.length());

            for (const auto& setter : prop->_setters) {
                setter(_entity.get(), prop, prop_data);
            }

            SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});
        }
        else {
            SetRawData(prop, {reinterpret_cast<const uint8*>(new_value.c_str()), new_value.length()});
        }

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity.get(), prop);
            }
        }
    }
}

template<typename T>
void Properties::SetValue(const Property* prop, const vector<T>& new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!prop->IsDisabled());
    FO_RUNTIME_ASSERT(prop->IsArray());
    FO_RUNTIME_ASSERT(prop->IsMutable() || prop->IsCoreProperty());

    PropertyRawData prop_data;

    if constexpr (std::same_as<T, string> || std::same_as<T, any_t>) {
        if (!new_value.empty()) {
            size_t data_size = sizeof(uint32);

            for (const auto& str : new_value) {
                data_size += sizeof(uint32) + static_cast<uint32>(str.length());
            }

            auto* buf = prop_data.Alloc(data_size);

            const auto arr_size = static_cast<uint32>(new_value.size());
            MemCopy(buf, &arr_size, sizeof(arr_size));
            buf += sizeof(uint32);

            for (const auto& str : new_value) {
                const auto str_size = static_cast<uint32>(str.length());
                MemCopy(buf, &str_size, sizeof(str_size));
                buf += sizeof(str_size);

                if (str_size != 0) {
                    MemCopy(buf, str.c_str(), str_size);
                    buf += str_size;
                }
            }
        }
    }
    else if constexpr (std::same_as<T, hstring>) {
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

        if (!new_value.empty()) {
            auto* buf = prop_data.Alloc(new_value.size() * sizeof(hstring::hash_t));

            for (const auto& hstr : new_value) {
                const auto hash = hstr.as_hash();
                MemCopy(buf, &hash, sizeof(hstring::hash_t));
                buf += sizeof(hstring::hash_t);
            }
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));

        if (!new_value.empty()) {
            prop_data.Pass(new_value.data(), new_value.size() * sizeof(T));
        }
    }

    if (prop->IsVirtual()) {
        FO_RUNTIME_ASSERT(_entity);
        FO_RUNTIME_ASSERT(!prop->_setters.empty());

        for (const auto& setter : prop->_setters) {
            setter(_entity.get(), prop, prop_data);
        }
    }
    else {
        FO_RUNTIME_ASSERT(prop->_complexDataIndex.has_value());

        if (!prop->_setters.empty() && _entity) {
            for (const auto& setter : prop->_setters) {
                setter(_entity.get(), prop, prop_data);
            }
        }

        SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity.get(), prop);
            }
        }
    }
}

FO_END_NAMESPACE();
