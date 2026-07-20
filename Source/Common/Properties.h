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

#pragma once

#include "Common.h"

#include "ConfigFile.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(PropertiesException);

class Entity;
class Property;
class PropertyRegistrator;
class Properties;

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

    [[nodiscard]] auto GetPtr() noexcept -> ptr<void>;
    [[nodiscard]] auto GetSize() const noexcept -> size_t { return _dataSize; }

    template<typename T>
    [[nodiscard]] auto GetPtrAs() noexcept -> ptr<T>
    {
        return GetPtr().reinterpret_as<T>();
    }

    template<typename T>
    [[nodiscard]] auto GetAs() noexcept -> T
    {
        FO_STRONG_ASSERT(sizeof(T) == _dataSize, "Property raw data size mismatch", sizeof(T), _dataSize);
        FO_STRONG_ASSERT(reinterpret_cast<uintptr_t>(GetPtr().get()) % alignof(T) == 0, "Property raw data is not aligned", sizeof(T), alignof(T));
        return *GetPtr().reinterpret_as<T>();
    }

    auto Alloc(size_t size) -> ptr<uint8_t>;
    void Set(nptr<const void> value, size_t size)
    {
        auto data = Alloc(size);

        if (size != 0) {
            FO_VERIFY_AND_THROW(value, "Source value pointer is null");
            MemCopy(data, value, size);
        }
    }

    template<typename T>
    void SetAs(T value)
    {
        auto target = Alloc(sizeof(T));
        MemCopy(target, &value, sizeof(T));
    }

    void Pass(span<const uint8_t> value);
    void Pass(nptr<const void> value, size_t size);
    void StoreIfPassed();

private:
    // Keep the over-aligned local buffer as the first member: placed at offset 0 the alignas is
    // satisfied without inserting any padding, so the struct's alignment matches its natural pointer
    // alignment and MSVC's C4324 (padding added due to alignment specifier) does not fire.
    alignas(std::max_align_t) uint8_t _localBuf[LOCAL_BUF_SIZE] {};
    size_t _dataSize {};
    bool _useDynamic {};
    unique_arr_ptr<uint8_t> _dynamicBuf {};
    nptr<void> _passedPtr {};
};

using PropertyGetCallback = function<PropertyRawData(nptr<Entity>, ptr<const Property>)>;
using PropertySetCallback = function<void(nptr<Entity>, ptr<const Property>, PropertyRawData&)>;
using PropertyPostSetCallback = function<void(nptr<Entity>, ptr<const Property>)>;

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

    [[nodiscard]] auto GetRegistrator() const noexcept -> ptr<const PropertyRegistrator> { return _registrator; }
    [[nodiscard]] auto GetName() const noexcept -> string_view { return _propName; }
    [[nodiscard]] auto GetNameWithoutComponent() const noexcept -> string_view { return _propNameWithoutComponent; }
    [[nodiscard]] auto GetComponentName() const noexcept -> string_view { return _componentName; }
    [[nodiscard]] auto GetRegIndex() const noexcept -> uint16_t { return _regIndex; }
    [[nodiscard]] auto GetBaseScriptFuncType() const noexcept -> string_view { return _scriptFuncType; }

    [[nodiscard]] auto GetBaseType() const noexcept -> const BaseTypeDesc& { return _baseType; }
    [[nodiscard]] auto GetBaseTypeName() const noexcept -> string_view { return _baseType.Name; }
    [[nodiscard]] auto GetBaseSize() const noexcept -> size_t { return _baseType.Size; }
    [[nodiscard]] auto GetDataAlignment() const noexcept -> size_t { return _dataAlignment; }
    [[nodiscard]] auto IsBaseTypeSimpleStruct() const noexcept -> bool { return _baseType.IsSimpleStruct; }
    [[nodiscard]] auto IsBaseTypeComplexStruct() const noexcept -> bool { return _baseType.IsComplexStruct; }
    [[nodiscard]] auto IsBaseTypeStruct() const noexcept -> bool { return _baseType.IsSimpleStruct || _baseType.IsComplexStruct; }
    [[nodiscard]] auto GetBaseTypeLayout() const noexcept -> const StructLayoutDesc& { return *_baseType.StructLayout; }
    [[nodiscard]] auto GetStructFirstType() const noexcept -> const BaseTypeDesc& { return _baseType.StructLayout->Fields.front().Type; }
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
    [[nodiscard]] auto IsBaseTypeHash() const noexcept -> bool { return _baseType.IsHashedString; }
    [[nodiscard]] auto IsBaseTypeFixedType() const noexcept -> bool { return _baseType.IsFixedType; }
    [[nodiscard]] auto IsBaseTypeEntityProto() const noexcept -> bool { return _baseType.IsEntityProto; }
    [[nodiscard]] auto IsBaseTypeProtoReference() const noexcept -> bool { return _baseType.IsFixedType || _baseType.IsEntityProto; }
    [[nodiscard]] auto IsBaseTypeRefType() const noexcept -> bool { return _baseType.IsRefType; }
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
    [[nodiscard]] auto GetDictKeyType() const noexcept -> const BaseTypeDesc& { return _dictKeyType; }
    [[nodiscard]] auto GetDictKeyTypeSize() const noexcept -> size_t { return _dictKeyType.Size; }
    [[nodiscard]] auto GetDictKeyTypeName() const noexcept -> string_view { return _dictKeyType.Name; }
    [[nodiscard]] auto GetViewTypeName() const noexcept -> string_view { return _viewTypeName; }

    [[nodiscard]] auto IsDisabled() const noexcept -> bool { return _isDisabled; }
    [[nodiscard]] auto IsComponentItself() const noexcept -> bool { return _isComponentItself; }
    [[nodiscard]] auto IsInComponent() const noexcept -> bool { return !_componentName.empty(); }
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
    [[nodiscard]] auto IsNullable() const noexcept -> bool { return _isNullable; }
    [[nodiscard]] auto IsTemporary() const noexcept -> bool { return (_isMutable || _isCoreProperty) && !_isPersistent; }

    [[nodiscard]] auto GetGetter() const noexcept -> ptr<const PropertyGetCallback> { return &_getter; }
    [[nodiscard]] auto GetSetters() const noexcept -> ptr<const vector<PropertySetCallback>> { return &_setters; }
    [[nodiscard]] auto GetPostSetters() const noexcept -> ptr<const vector<PropertyPostSetCallback>> { return &_postSetters; }

    void SetGetter(PropertyGetCallback getter) const;
    void AddSetter(PropertySetCallback setter) const;
    void AddPostSetter(PropertyPostSetCallback setter) const;

private:
    explicit Property(ptr<const PropertyRegistrator> registrator);

    ptr<const PropertyRegistrator> _registrator;

    mutable PropertyGetCallback _getter {};
    mutable vector<PropertySetCallback> _setters {};
    mutable vector<PropertyPostSetCallback> _postSetters {};

    string _propName {};
    string _propNameWithoutComponent {};
    string _componentName {};
    string _scriptFuncType {};

    BaseTypeDesc _baseType {};

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
    BaseTypeDesc _dictKeyType {};

    string _viewTypeName {};

    bool _isDisabled {};
    bool _isComponentItself {};
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
    bool _isNullable {};
    bool _containsFloat {};
    uint16_t _regIndex {};
    size_t _dataAlignment {1};
    optional<size_t> _podDataOffset {};
    optional<size_t> _complexDataIndex {};
};

class Properties final
{
    friend class PropertyRegistrator;
    friend class Property;

public:
    struct StoreDataCache
    {
        vector<nptr<const uint8_t>> Data {};
        vector<uint32_t> Sizes {};
        vector<uint16_t> PropertyIndices {};
        uint32_t Revision {};
    };

    struct StoredData
    {
        ptr<const vector<nptr<const uint8_t>>> Data;
        ptr<const vector<uint32_t>> Sizes;
    };

    struct OverlayEntry
    {
        uint16_t PropRegIndex {};
        uint16_t Padding {};
        uint32_t DataOffset {};
        uint32_t DataSize {};
    };

    Properties() = delete;
    explicit Properties(ptr<const PropertyRegistrator> registrator, nptr<const Properties> base = nullptr) noexcept;
    Properties(const Properties& other) = delete;
    Properties(Properties&&) noexcept = default;
    auto operator=(const Properties& other) noexcept = delete;
    auto operator=(Properties&&) noexcept = delete;
    ~Properties() = default;

    [[nodiscard]] auto GetRegistrator() const noexcept -> ptr<const PropertyRegistrator> { return _registrator; }
    [[nodiscard]] auto GetEntity() const noexcept -> nptr<Entity> { return _entity; }
    [[nodiscard]] auto HasBaseProperties() const noexcept -> bool { return !!_baseProps; }
    [[nodiscard]] auto GetBaseProperties() const noexcept -> nptr<const Properties> { return _baseProps; }
    [[nodiscard]] auto Copy() const noexcept -> Properties;
    [[nodiscard]] auto GetRawData(ptr<const Property> prop) const noexcept -> span<const uint8_t>;
    [[nodiscard]] auto GetRawDataSize(ptr<const Property> prop) const noexcept -> size_t;
    [[nodiscard]] auto GetPlainDataValueAsInt(ptr<const Property> prop) const -> int32_t;
    [[nodiscard]] auto GetPlainDataValueAsAny(ptr<const Property> prop) const -> any_t;
    [[nodiscard]] auto GetValueAsInt(int32_t property_index) const -> int32_t;
    [[nodiscard]] auto GetValueAsAny(int32_t property_index) const -> any_t;
    [[nodiscard]] auto SavePropertyToText(ptr<const Property> prop) const -> string;
    [[nodiscard]] auto SaveToText(nptr<const Properties> base) const -> map<string, string>;
    [[nodiscard]] auto CompareData(const Properties& other, const_span<ptr<const Property>> ignore_props, bool ignore_temporary) const -> bool;

    void AllocData() noexcept;
    void SetEntity(ptr<Entity> entity) const noexcept { _entity = entity; }
    void CopyFrom(const Properties& other) noexcept;
    void ValidateForRawData(ptr<const Property> prop) const noexcept(false);
    void ApplyFromText(const map<string, string>& key_values);
    void ApplyFromText(const map<string_view, string_view>& key_values);
    void ApplyPropertyFromText(ptr<const Property> prop, string_view text);
    void StoreAllData(vector<uint8_t>& all_data, set<hstring>& str_hashes) const;
    void RestoreAllData(const vector<uint8_t>& all_data);
    auto StoreData(bool with_protected) const -> StoredData;
    void RestoreData(const vector<nptr<const uint8_t>>& all_data, const vector<uint32_t>& all_data_sizes);
    void RestoreData(const vector<vector<uint8_t>>& all_data);
    void CopyRawData(ptr<const Property> prop, PropertyRawData& prop_data) const noexcept;
    void SetRawData(ptr<const Property> prop, span<const uint8_t> raw_data) noexcept;
    void SetValueFromData(ptr<const Property> prop, PropertyRawData& prop_data);
    void SetPlainDataValueAsInt(ptr<const Property> prop, int32_t value);
    void SetPlainDataValueAsAny(ptr<const Property> prop, const any_t& value);
    void SetValueAsInt(int32_t property_index, int32_t value);
    void SetValueAsAny(int32_t property_index, const any_t& value);
    void SetValueAsIntProps(int32_t property_index, int32_t value);
    void SetValueAsAnyProps(int32_t property_index, const any_t& value);
    auto ResolveHash(hstring::hash_t h) const -> hstring;
    auto ResolveHash(hstring::hash_t h, nptr<bool> failed) const noexcept -> hstring;

    auto FindOverlayEntry(ptr<const Property> prop) const noexcept -> nptr<const OverlayEntry>;
    auto FindOverlayEntry(ptr<const Property> prop) noexcept -> nptr<OverlayEntry>;
    void EnsureOverlayEntryIndex() noexcept;
    void RebuildOverlayEntryIndex() noexcept;
    auto IsOverlayPropertyIncluded(ptr<const Property> prop, bool with_protected) const noexcept -> bool;
    void CloneOwnDataFrom(const Properties& other) noexcept;
    void RebuildOverlayFromFullData(const Properties& other) noexcept;
    auto RepackOverlayData(size_t min_capacity) noexcept -> void;
    auto AllocOverlayData(size_t data_size, size_t data_alignment) noexcept -> uint32_t;
    void ResetComplexData() noexcept;
    void RemoveSyncedOverlayEntries() noexcept;
    void RemoveOverlayEntry(ptr<const Property> prop) noexcept;
    void ResetOverlayData() noexcept;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
    [[nodiscard]] auto GetValue(ptr<const Property> prop) const -> T;
    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto GetValue(ptr<const Property> prop) const -> T;
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    [[nodiscard]] auto GetValue(ptr<const Property> prop) const -> T;
    template<typename T>
        requires(vector_collection<T>)
    [[nodiscard]] auto GetValue(ptr<const Property> prop) const -> T;
    template<typename T>
        requires(map_collection<T>)
    [[nodiscard]] auto GetValue(ptr<const Property> prop) const -> T = delete;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
    [[nodiscard]] auto GetValueFast(ptr<const Property> prop) const noexcept -> T;
    template<typename T>
        requires(std::same_as<T, hstring>)
    [[nodiscard]] auto GetValueFast(ptr<const Property> prop) const noexcept -> T;
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    [[nodiscard]] auto GetValueFast(ptr<const Property> prop) const noexcept -> string_view;
    template<typename T>
        requires(vector_collection<T>)
    [[nodiscard]] auto GetValueFast(ptr<const Property> prop) const noexcept -> T;
    template<typename T>
        requires(map_collection<T>)
    [[nodiscard]] auto GetValueFast(ptr<const Property> prop) const -> T = delete;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
    void SetValue(ptr<const Property> prop, T new_value);
    template<typename T>
        requires(std::same_as<T, hstring>)
    void SetValue(ptr<const Property> prop, T new_value);
    template<typename T>
        requires(std::same_as<T, string> || std::same_as<T, any_t>)
    void SetValue(ptr<const Property> prop, const T& new_value);
    template<typename T>
    void SetValue(ptr<const Property> prop, const vector<T>& new_value);
    template<typename T, typename U>
    void SetValue(ptr<const Property> prop, const map<T, U>& new_value) = delete;

    void SetValue(ptr<const Property> prop, PropertyRawData& prop_data);

private:
    auto ShouldUseOverlayEntryIndex(size_t entry_count) const noexcept -> bool;
    void ReleaseOverlayEntryIndex() noexcept;
    auto MakeOverlayPackOrder() const noexcept -> vector<size_t>;
    auto IsRawDataEqual(ptr<const Property> prop, span<const uint8_t> raw_data) const noexcept -> bool;
    static void ValidateFiniteRawData(ptr<const Property> prop, span<const uint8_t> raw_data);

    ptr<const PropertyRegistrator> _registrator;
    nptr<const Properties> _baseProps {};

    unique_arr_ptr<uint8_t> _podData {};
    unique_arr_ptr<pair<unique_arr_ptr<uint8_t>, size_t>> _complexData {};

    small_vector<OverlayEntry, 16> _overlayEntries {};
    vector<int32_t> _overlayEntryIndex {};
    unique_arr_ptr<uint8_t> _overlayData {};
    size_t _overlayDataSize {};
    size_t _overlayDataCapacity {};
    size_t _overlayGarbageSize {};

    uint32_t _storeDataRevision {1};
    mutable optional<StoreDataCache> _storeDataCaches[2] {};
    mutable nptr<Entity> _entity {};
};

class PropertyRegistrator final
{
    friend class Properties;
    friend class Property;

public:
    PropertyRegistrator() = delete;
    PropertyRegistrator(string_view type_name, EngineSideKind side, ptr<HashResolver> hash_resolver, ptr<NameResolver> name_resolver);
    PropertyRegistrator(const PropertyRegistrator&) = delete;
    PropertyRegistrator(PropertyRegistrator&&) noexcept = default;
    auto operator=(const PropertyRegistrator&) = delete;
    auto operator=(PropertyRegistrator&&) noexcept = delete;
    ~PropertyRegistrator();

    [[nodiscard]] auto GetTypeName() const noexcept -> hstring { return _typeName; }
    [[nodiscard]] auto GetTypeNamePlural() const noexcept -> hstring { return _typeNamePlural; }
    [[nodiscard]] auto GetSide() const noexcept -> EngineSideKind { return _side; }
    [[nodiscard]] auto GetPropertiesCount() const noexcept -> size_t { return _registeredProperties.size(); }
    [[nodiscard]] auto FindProperty(string_view property_name) const -> nptr<const Property>;
    [[nodiscard]] auto GetPropertyByIndex(int32_t property_index) const noexcept -> nptr<const Property>;
    [[nodiscard]] auto GetPropertyByIndexUnsafe(size_t property_index) const noexcept -> ptr<const Property>;
    [[nodiscard]] auto GetWholeDataSize() const noexcept -> size_t { return _wholePodDataSize; }
    [[nodiscard]] auto GetPropertyGroups() const noexcept -> map<string, vector<ptr<const Property>>>;
    [[nodiscard]] auto GetComponents() const noexcept -> const unordered_map<string, ptr<const Property>>& { return _registeredComponents; }
    [[nodiscard]] auto GetHashResolver() const noexcept -> ptr<HashResolver> { return _hashResolver; }
    [[nodiscard]] auto GetNameResolver() const noexcept -> ptr<NameResolver> { return _nameResolver; }

    auto RegisterProperty(const initializer_list<string_view>& tokens) -> ptr<const Property> { return RegisterProperty({tokens.begin(), tokens.end()}); }
    auto RegisterProperty(const span<const string_view>& tokens) -> ptr<const Property>;

private:
    struct DataPropertyEntry
    {
        nptr<Property> Prop {};
        uint32_t DataIndex {};
        uint16_t DataSize {};
        bool IsPlain {};
    };

    const hstring _typeName;
    const hstring _typeNamePlural;
    const EngineSideKind _side;
    const hstring _propMigrationRuleName;
    mutable ptr<HashResolver> _hashResolver;
    mutable ptr<NameResolver> _nameResolver;
    vector<unique_nptr<Property>> _registeredProperties {};
    vector<DataPropertyEntry> _dataProperties {};
    vector<ptr<Property>> _textProperties {};
    vector<ptr<Property>> _hashProperties {};
    unordered_map<string, ptr<const Property>> _registeredPropertiesLookup {};
    unordered_map<string, ptr<const Property>> _registeredComponents {};
    map<string, vector<pair<ptr<const Property>, int32_t>>> _propertyGroups {};

    // PlainData info
    size_t _wholePodDataSize {};
    vector<bool> _publicPodDataSpace {};
    vector<bool> _protectedPodDataSpace {};
    vector<bool> _privatePodDataSpace {};

    // Complex types info
    vector<ptr<Property>> _complexProperties {};
    vector<uint16_t> _publicComplexDataProps {};
    unordered_set<uint16_t> _publicComplexDataPropsLookup {};
    vector<uint16_t> _protectedComplexDataProps {};
    vector<uint16_t> _publicProtectedComplexDataProps {};
    unordered_set<uint16_t> _publicProtectedComplexDataPropsLookup {};
};

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
auto Properties::GetValue(ptr<const Property> prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(alignof(T) <= MAX_SERIALIZED_ALIGNMENT, "Property value type is over-aligned for the MAX_SERIALIZED_ALIGNMENT raw-data layout contract");

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(T), "Property base size does not match requested value type", prop->GetName(), prop->GetBaseSize(), sizeof(T));
    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity, prop);
            return prop_data.GetAs<T>();
        }
        else {
            return {};
        }
    }

    auto raw_data = GetRawData(prop);
    FO_VERIFY_AND_THROW(raw_data.size() == sizeof(T), "Property raw data size does not match requested value type", prop->GetName(), raw_data.size(), sizeof(T));
    auto raw_data_ptr = make_ptr(raw_data.data());
    return *raw_data_ptr.reinterpret_as<T>();
}

template<typename T>
    requires(std::same_as<T, hstring>)
auto Properties::GetValue(ptr<const Property> prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hstring::hash_t), "Hash property base size does not match hash storage size", prop->GetName(), prop->GetBaseSize(), sizeof(hstring::hash_t));
    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");
    FO_VERIFY_AND_THROW(prop->IsBaseTypeHash(), "Property base type is not hash");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity, prop);
            auto hash = prop_data.GetAs<hstring::hash_t>();
            return ResolveHash(hash);
        }
        else {
            return {};
        }
    }

    auto raw_data = GetRawData(prop);
    FO_VERIFY_AND_THROW(raw_data.size() == sizeof(hstring::hash_t), "Hash property raw data size does not match hash storage size", prop->GetName(), raw_data.size(), sizeof(hstring::hash_t));
    auto raw_data_ptr = make_ptr(raw_data.data());
    return ResolveHash(*raw_data_ptr.reinterpret_as<hstring::hash_t>());
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
auto Properties::GetValue(ptr<const Property> prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->IsString(), "Property base type is not string");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");

        if (prop->_getter) {
            PropertyRawData prop_data = prop->_getter(_entity, prop);
            return string(prop_data.GetPtrAs<char>().get(), prop_data.GetSize());
        }
        else {
            return {};
        }
    }

    auto raw_data = GetRawData(prop);

    if (raw_data.empty()) {
        return {};
    }

    return string(span_to_string(raw_data));
}

template<typename T>
    requires(vector_collection<T>)
auto Properties::GetValue(ptr<const Property> prop) const -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->IsArray(), "Property is not an array");

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");

        if (prop->_getter) {
            prop_data = prop->_getter(_entity, prop);
        }
    }
    else {
        prop_data.Pass(GetRawData(prop));
    }

    span<const uint8_t> data = {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()};

    T result;

    if (!data.empty()) {
        if constexpr (std::same_as<T, vector<string>> || std::same_as<T, vector<any_t>>) {
            FO_VERIFY_AND_THROW(prop->IsArrayOfString(), "Property is not an array of strings");

            size_t data_pos = 0;
            FO_VERIFY_AND_THROW(data_pos + sizeof(uint32_t) <= data.size(), "Array length prefix exceeds available data");
            auto arr_size_data = make_ptr(data.data());
            uint32_t arr_size = *arr_size_data.reinterpret_as<uint32_t>();
            data_pos += sizeof(arr_size);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for (uint32_t i = 0; i < arr_size; i++) {
                data_pos = align_up(data_pos, sizeof(uint32_t));
                FO_VERIFY_AND_THROW(data_pos + sizeof(uint32_t) <= data.size(), "String length prefix exceeds available data");
                auto str_size_data = make_ptr(data.data() + data_pos);
                uint32_t str_size = *str_size_data.reinterpret_as<uint32_t>();
                data_pos += sizeof(str_size);
                FO_VERIFY_AND_THROW(data_pos + str_size <= data.size(), "String payload exceeds available data");
                result.emplace_back(string(span_to_string(data.subspan(data_pos, str_size))));
                data_pos += str_size;
            }
        }
        else if constexpr (std::same_as<T, vector<hstring>>) {
            FO_VERIFY_AND_THROW(prop->IsBaseTypeHash(), "Property base type is not hash");
            FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hstring::hash_t), "Hash array property base size does not match hash storage size", prop->GetName(), prop->GetBaseSize(), sizeof(hstring::hash_t));

            FO_VERIFY_AND_THROW(data.size() % sizeof(hstring::hash_t) == 0, "Hash array data size is not aligned to hash storage size");
            auto arr_size = data.size() / sizeof(hstring::hash_t);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for (size_t i = 0; i < arr_size; i++) {
                auto hash_data = make_ptr(data.data() + numeric_cast<size_t>(i) * sizeof(hstring::hash_t));
                hstring::hash_t hash = *hash_data.reinterpret_as<hstring::hash_t>();
                hstring hvalue = ResolveHash(hash);
                result.emplace_back(hvalue);
            }
        }
        else {
            FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(typename T::value_type), "Array property base size does not match requested element type", prop->GetName(), prop->GetBaseSize(), sizeof(typename T::value_type));
            FO_VERIFY_AND_THROW(data.size() % prop->GetBaseSize() == 0, "Array property raw data size is not aligned to the property base size", prop->GetName(), data.size(), prop->GetBaseSize());
            size_t arr_size = data.size() / prop->GetBaseSize();
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);
            result.resize(arr_size);
            MemCopy(result.data(), data.data(), data.size());
        }
    }

    return result;
}

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
auto Properties::GetValueFast(ptr<const Property> prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(alignof(T) <= MAX_SERIALIZED_ALIGNMENT, "Property value type is over-aligned for the MAX_SERIALIZED_ALIGNMENT raw-data layout contract");

    FO_STRONG_ASSERT(!prop->IsDisabled(), "Disabled property used in fast value getter", prop->GetName());
    FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(T), "Property base size mismatch in fast value getter", prop->GetName(), sizeof(T), prop->GetBaseSize());
    FO_STRONG_ASSERT(prop->IsPlainData(), "Property is not plain data in fast value getter", prop->GetName());
    FO_STRONG_ASSERT(!prop->IsVirtual(), "Virtual property used in fast value getter", prop->GetName());

    auto raw_data = GetRawData(prop);
    FO_STRONG_ASSERT(raw_data.size() == sizeof(T), "Property raw data size mismatch in fast value getter", prop->GetName(), sizeof(T), raw_data.size());
    auto raw_data_ptr = make_ptr(raw_data.data());
    return *raw_data_ptr.reinterpret_as<T>();
}

template<typename T>
    requires(std::same_as<T, hstring>)
auto Properties::GetValueFast(ptr<const Property> prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled(), "Disabled property used in hstring fast value getter", prop->GetName());
    FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t), "Property hash base size mismatch in hstring fast value getter", prop->GetName(), sizeof(hstring::hash_t), prop->GetBaseSize());
    FO_STRONG_ASSERT(prop->IsPlainData(), "Property is not plain data in hstring fast value getter", prop->GetName());
    FO_STRONG_ASSERT(prop->IsBaseTypeHash(), "Property is not hash data in hstring fast value getter", prop->GetName());
    FO_STRONG_ASSERT(!prop->IsVirtual(), "Virtual property used in hstring fast value getter", prop->GetName());

    auto raw_data = GetRawData(prop);
    FO_STRONG_ASSERT(raw_data.size() == sizeof(hstring::hash_t), "Property raw hash data size mismatch in hstring fast value getter", prop->GetName(), sizeof(hstring::hash_t), raw_data.size());
    auto raw_data_ptr = make_ptr(raw_data.data());
    return ResolveHash(*raw_data_ptr.reinterpret_as<hstring::hash_t>(), nullptr);
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
auto Properties::GetValueFast(ptr<const Property> prop) const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled(), "Disabled property used in string fast value getter", prop->GetName());
    FO_STRONG_ASSERT(prop->IsString(), "Property is not string data in fast value getter", prop->GetName());
    FO_STRONG_ASSERT(!prop->IsVirtual(), "Virtual property used in string fast value getter", prop->GetName());

    auto raw_data = GetRawData(prop);

    if (raw_data.empty()) {
        return {};
    }

    return span_to_string(raw_data);
}

template<typename T>
    requires(vector_collection<T>)
auto Properties::GetValueFast(ptr<const Property> prop) const noexcept -> T
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(!prop->IsDisabled(), "Disabled property used in array fast value getter", prop->GetName());
    FO_STRONG_ASSERT(prop->IsArray(), "Property is not array data in fast value getter", prop->GetName());
    FO_STRONG_ASSERT(!prop->IsVirtual(), "Virtual property used in array fast value getter", prop->GetName());

    PropertyRawData prop_data;

    prop_data.Pass(GetRawData(prop));

    if (prop_data.GetSize() == 0) {
        return {};
    }

    span<const uint8_t> data = {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()};

    T result;

    if (!data.empty()) {
        if constexpr (std::same_as<T, vector<string>> || std::same_as<T, vector<any_t>>) {
            FO_STRONG_ASSERT(prop->IsArrayOfString(), "Property is not string array data in fast value getter", prop->GetName());

            size_t data_pos = 0;
            FO_VERIFY_AND_THROW(data_pos + sizeof(uint32_t) <= data.size(), "Array length prefix exceeds available data");
            auto arr_size_data = make_ptr(data.data());
            uint32_t arr_size = *arr_size_data.reinterpret_as<uint32_t>();
            data_pos += sizeof(arr_size);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for (uint32_t i = 0; i < arr_size; i++) {
                data_pos = align_up(data_pos, sizeof(uint32_t));
                FO_VERIFY_AND_THROW(data_pos + sizeof(uint32_t) <= data.size(), "String length prefix exceeds available data");
                auto str_size_data = make_ptr(data.data() + data_pos);
                uint32_t str_size = *str_size_data.reinterpret_as<uint32_t>();
                data_pos += sizeof(str_size);
                FO_VERIFY_AND_THROW(data_pos + str_size <= data.size(), "String payload exceeds available data");
                result.emplace_back(string(span_to_string(data.subspan(data_pos, str_size))));
                data_pos += str_size;
            }
        }
        else if constexpr (std::same_as<T, vector<hstring>>) {
            FO_STRONG_ASSERT(prop->IsBaseTypeHash(), "Property is not hash array data in fast value getter", prop->GetName());
            FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t), "Property hash array base size mismatch in fast value getter", prop->GetName(), sizeof(hstring::hash_t), prop->GetBaseSize());

            FO_VERIFY_AND_THROW(data.size() % sizeof(hstring::hash_t) == 0, "Hash array data size is not aligned to hash storage size");
            auto arr_size = data.size() / sizeof(hstring::hash_t);
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);

            for (size_t i = 0; i < arr_size; i++) {
                auto hash_data = make_ptr(data.data() + numeric_cast<size_t>(i) * sizeof(hstring::hash_t));
                hstring::hash_t hash = *hash_data.reinterpret_as<hstring::hash_t>();
                hstring hvalue = ResolveHash(hash, nullptr);
                result.emplace_back(hvalue);
            }
        }
        else {
            FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(typename T::value_type), "Property array base size mismatch in fast value getter", prop->GetName(), prop->GetBaseSize(), sizeof(typename T::value_type));
            FO_STRONG_ASSERT(data.size() % prop->GetBaseSize() == 0, "Property array raw data size is not aligned to base size", prop->GetName(), data.size(), prop->GetBaseSize());
            size_t arr_size = data.size() / prop->GetBaseSize();
            result.reserve(arr_size != 0 ? arr_size + 8 : 0);
            result.resize(arr_size);
            MemCopy(result.data(), data.data(), data.size());
        }
    }

    return result;
}

template<typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || some_property_plain_type<T> || some_strong_type<T>)
void Properties::SetValue(ptr<const Property> prop, T new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    static_assert(alignof(T) <= MAX_SERIALIZED_ALIGNMENT, "Property value type is over-aligned for the MAX_SERIALIZED_ALIGNMENT raw-data layout contract");

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(T), "Property base size does not match assigned value type", prop->GetName(), prop->GetBaseSize(), sizeof(T));
    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");
    FO_VERIFY_AND_THROW(prop->IsMutable() || prop->IsCoreProperty(), "Property must be mutable or core before raw data update");

    auto new_value_ptr = make_ptr(&new_value);
    ValidateFiniteRawData(prop, {new_value_ptr.template reinterpret_as<uint8_t>().get(), sizeof(T)});

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");
        FO_VERIFY_AND_THROW(!prop->_setters.empty(), "Virtual property has no setter while assigning a plain value", prop->GetName(), sizeof(T));

        PropertyRawData prop_data;
        prop_data.SetAs<T>(new_value);

        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        auto raw_data = GetRawData(prop);
        FO_VERIFY_AND_THROW(raw_data.size() == sizeof(T), "Property raw data size does not match assigned value type", prop->GetName(), raw_data.size(), sizeof(T));
        auto raw_data_ptr = make_ptr(raw_data.data());
        T cur_value = *raw_data_ptr.reinterpret_as<T>();
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
                    setter(_entity, prop, prop_data);
                }

                ValidateFiniteRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
                SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
            }
            else {
                SetRawData(prop, {new_value_ptr.template reinterpret_as<uint8_t>().get(), sizeof(T)});
            }

            if (_entity) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity, prop);
                }
            }
        }
    }
}

template<typename T>
    requires(std::same_as<T, hstring>)
void Properties::SetValue(ptr<const Property> prop, T new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hstring::hash_t), "Hash property base size does not match assigned hash storage size", prop->GetName(), prop->GetBaseSize(), sizeof(hstring::hash_t));
    FO_VERIFY_AND_THROW(prop->IsPlainData(), "Property is not plain data");
    FO_VERIFY_AND_THROW(prop->IsBaseTypeHash(), "Property base type is not hash");
    FO_VERIFY_AND_THROW(prop->IsMutable() || prop->IsCoreProperty(), "Property must be mutable or core before raw data update");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");
        FO_VERIFY_AND_THROW(!prop->_setters.empty(), "Virtual hash property has no setter while assigning a hashed value", prop->GetName(), sizeof(hstring::hash_t));

        PropertyRawData prop_data;
        prop_data.SetAs<hstring::hash_t>(new_value.as_hash());

        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        auto new_value_hash = new_value.as_hash();
        auto raw_data = GetRawData(prop);
        FO_VERIFY_AND_THROW(raw_data.size() == sizeof(hstring::hash_t), "Hash property raw data size does not match assigned hash storage size", prop->GetName(), raw_data.size(), sizeof(hstring::hash_t));
        auto raw_data_ptr = make_ptr(raw_data.data());
        hstring::hash_t cur_value_hash = *raw_data_ptr.reinterpret_as<hstring::hash_t>();

        if (new_value_hash != cur_value_hash) {
            if (!prop->_setters.empty() && _entity) {
                PropertyRawData prop_data;
                prop_data.SetAs<hstring::hash_t>(new_value_hash);

                for (const auto& setter : prop->_setters) {
                    setter(_entity, prop, prop_data);
                }

                SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
            }
            else {
                auto new_value_hash_ptr = make_ptr(&new_value_hash);
                SetRawData(prop, {new_value_hash_ptr.template reinterpret_as<uint8_t>().get(), sizeof(hstring::hash_t)});
            }

            if (_entity) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity, prop);
                }
            }
        }
    }
}

template<typename T>
    requires(std::same_as<T, string> || std::same_as<T, any_t>)
void Properties::SetValue(ptr<const Property> prop, const T& new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->IsString(), "Property base type is not string");
    FO_VERIFY_AND_THROW(prop->IsMutable() || prop->IsCoreProperty(), "Property must be mutable or core before raw data update");

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");
        FO_VERIFY_AND_THROW(!prop->_setters.empty(), "Virtual string property has no setter while assigning a string value", prop->GetName(), new_value.length());

        PropertyRawData prop_data;
        prop_data.Pass(new_value.c_str(), new_value.length());

        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Complex property has no complex data index");

        if (!prop->_setters.empty() && _entity) {
            PropertyRawData prop_data;
            prop_data.Pass(new_value.c_str(), new_value.length());

            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }

            SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
        }
        else {
            SetRawData(prop, make_const_span(new_value));
        }

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity, prop);
            }
        }
    }
}

template<typename T>
void Properties::SetValue(ptr<const Property> prop, const vector<T>& new_value)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!prop->IsDisabled(), "Property is disabled");
    FO_VERIFY_AND_THROW(prop->IsArray(), "Property is not an array");
    FO_VERIFY_AND_THROW(prop->IsMutable() || prop->IsCoreProperty(), "Property must be mutable or core before raw data update");

    PropertyRawData prop_data;

    if constexpr (std::same_as<T, string> || std::same_as<T, any_t>) {
        if (!new_value.empty()) {
            size_t data_size = sizeof(uint32_t);

            for (const auto& str : new_value) {
                data_size = align_up(data_size, sizeof(uint32_t));
                data_size += sizeof(uint32_t) + static_cast<uint32_t>(str.length());
            }

            auto buf = prop_data.Alloc(data_size);
            MemFill(buf, 0, data_size);
            size_t data_pos = 0;

            uint32_t arr_size = static_cast<uint32_t>(new_value.size());
            *buf.offset(data_pos).reinterpret_as<uint32_t>() = arr_size;
            data_pos += sizeof(arr_size);

            for (const auto& str : new_value) {
                data_pos = align_up(data_pos, sizeof(uint32_t));
                uint32_t str_size = static_cast<uint32_t>(str.length());
                *buf.offset(data_pos).reinterpret_as<uint32_t>() = str_size;
                data_pos += sizeof(str_size);

                if (str_size != 0) {
                    MemCopy(buf.offset(data_pos), str.data(), str_size);
                    data_pos += str_size;
                }
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Written byte count does not match allocated size");
        }
    }
    else if constexpr (std::same_as<T, hstring>) {
        FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hstring::hash_t), "Hash vector property base size does not match hash storage size", prop->GetName(), prop->GetBaseSize(), sizeof(hstring::hash_t));

        if (!new_value.empty()) {
            size_t data_size = new_value.size() * sizeof(hstring::hash_t);
            auto buf = prop_data.Alloc(data_size);
            size_t data_pos = 0;

            for (const auto& hstr : new_value) {
                *buf.offset(data_pos).reinterpret_as<hstring::hash_t>() = hstr.as_hash();
                data_pos += sizeof(hstring::hash_t);
            }

            FO_VERIFY_AND_THROW(data_pos == data_size, "Written byte count does not match allocated size");
        }
    }
    else {
        FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(T), "Vector property base size does not match element storage size", prop->GetName(), prop->GetBaseSize(), sizeof(T));

        if (!new_value.empty()) {
            prop_data.Pass(new_value.data(), new_value.size() * sizeof(T));
        }
    }

    ValidateFiniteRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});

    if (prop->IsVirtual()) {
        FO_VERIFY_AND_THROW(_entity, "Missing entity instance");
        FO_VERIFY_AND_THROW(!prop->_setters.empty(), "Virtual array property has no setter while assigning an array value", prop->GetName(), new_value.size());

        for (const auto& setter : prop->_setters) {
            setter(_entity, prop, prop_data);
        }
    }
    else {
        FO_VERIFY_AND_THROW(prop->_complexDataIndex.has_value(), "Complex property has no complex data index");

        if (!prop->_setters.empty() && _entity) {
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }

            // Setters can rewrite the raw payload, so the mutated data must pass validation again
            ValidateFiniteRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});
        }

        SetRawData(prop, {prop_data.GetPtrAs<uint8_t>().get(), prop_data.GetSize()});

        if (_entity) {
            for (const auto& setter : prop->_postSetters) {
                setter(_entity, prop);
            }
        }
    }
}

FO_END_NAMESPACE
