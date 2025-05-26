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

enum class PropertiesRelationType
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

    void Pass(const_span<uint8> value) noexcept;
    void Pass(const void* value, size_t size) noexcept;
    void StoreIfPassed() noexcept;

private:
    size_t _dataSize {};
    bool _useDynamic {};
    uint8 _localBuf[LOCAL_BUF_SIZE] {};
    unique_ptr<uint8[]> _dynamicBuf {};
    void* _passedPtr {};
};

using PropertyGetCallback = std::function<PropertyRawData(Entity*, const Property*)>;
using PropertySetCallback = std::function<void(Entity*, const Property*, PropertyRawData&)>;
using PropertyPostSetCallback = std::function<void(Entity*, const Property*)>;

class Property final
{
    friend class PropertyRegistrator;
    friend class Properties;
    friend class SafeAlloc;

public:
    static constexpr size_t INVALID_DATA_MARKER = static_cast<size_t>(-1);

    enum class AccessType
    {
        PrivateCommon = 0x0010,
        PrivateClient = 0x0020,
        PrivateServer = 0x0040,
        Public = 0x0100,
        PublicModifiable = 0x0200,
        PublicFullModifiable = 0x0400,
        Protected = 0x1000,
        ProtectedModifiable = 0x2000,
        VirtualPrivateCommon = 0x0011,
        VirtualPrivateClient = 0x0021,
        VirtualPrivateServer = 0x0041,
        VirtualPublic = 0x0101,
        VirtualProtected = 0x1001,

        VirtualMask = 0x000F,
        PrivateMask = 0x00F0,
        PublicMask = 0x0F00,
        ProtectedMask = 0xF000,
        ClientOnlyMask = 0x0020,
        ServerOnlyMask = 0x0040,
        ModifiableMask = 0x2600,
    };

    Property(const Property&) = delete;
    Property(Property&&) noexcept = default;
    auto operator=(const Property&) -> Property& = delete;
    auto operator=(Property&&) noexcept -> Property& = default;
    ~Property() = default;

    [[nodiscard]] auto GetRegistrator() const noexcept -> const PropertyRegistrator* { return _registrator; }
    [[nodiscard]] auto GetName() const noexcept -> const string& { return _propName; }
    [[nodiscard]] auto GetNameWithoutComponent() const noexcept -> const string& { return _propNameWithoutComponent; }
    [[nodiscard]] auto GetComponent() const noexcept -> const hstring& { return _component; }
    [[nodiscard]] auto GetRegIndex() const noexcept -> uint16 { return _regIndex; }
    [[nodiscard]] auto GetAccess() const noexcept -> AccessType { return _accessType; }
    [[nodiscard]] auto GetBaseScriptFuncType() const noexcept -> const string& { return _scriptFuncType; }

    [[nodiscard]] auto GetBaseTypeInfo() const noexcept -> const BaseTypeInfo& { return _baseType; }
    [[nodiscard]] auto GetBaseTypeName() const noexcept -> const string& { return _baseType.TypeName; }
    [[nodiscard]] auto GetBaseSize() const noexcept -> size_t { return _baseType.Size; }
    [[nodiscard]] auto IsBaseTypeStruct() const noexcept -> bool { return _baseType.IsStruct; }
    [[nodiscard]] auto GetBaseTypeLayout() const noexcept { return _baseType.StructLayout; }
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
    [[nodiscard]] auto GetFullTypeName() const noexcept -> const string& { return _asFullTypeName; }

    [[nodiscard]] auto IsDisabled() const noexcept -> bool { return _isDisabled; }
    [[nodiscard]] auto IsVirtual() const noexcept -> bool { return _isVirtual; }
    [[nodiscard]] auto IsReadOnly() const noexcept -> bool { return _isReadOnly; }
    [[nodiscard]] auto IsTemporary() const noexcept -> bool { return _isTemporary; }
    [[nodiscard]] auto IsHistorical() const noexcept -> bool { return _isHistorical; }
    [[nodiscard]] auto IsNullGetterForProto() const noexcept -> bool { return _isNullGetterForProto; }

    [[nodiscard]] auto GetGetter() const noexcept -> auto& { return _getter; }
    [[nodiscard]] auto GetSetters() const noexcept -> auto& { return _setters; }
    [[nodiscard]] auto GetPostSetters() const noexcept -> auto& { return _postSetters; }

    void SetGetter(PropertyGetCallback getter) const;
    void AddSetter(PropertySetCallback setter) const;
    void AddPostSetter(PropertyPostSetCallback setter) const;

private:
    explicit Property(const PropertyRegistrator* registrator);

    const PropertyRegistrator* _registrator;

    mutable PropertyGetCallback _getter {};
    mutable vector<PropertySetCallback> _setters {};
    mutable vector<PropertyPostSetCallback> _postSetters {};

    string _propName {};
    string _propNameWithoutComponent {};
    hstring _component {};
    string _scriptFuncType {};
    AccessType _accessType {};

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

    string _asFullTypeName {};

    bool _isDisabled {};
    bool _isVirtual {};
    bool _isReadOnly {};
    bool _checkMinValue {};
    bool _checkMaxValue {};
    int64 _minValueI {};
    int64 _maxValueI {};
    double _minValueF {};
    double _maxValueF {};
    bool _isTemporary {};
    bool _isHistorical {};
    bool _isNullGetterForProto {};
    uint16 _regIndex {};
    size_t _podDataOffset {INVALID_DATA_MARKER};
    size_t _complexDataIndex {INVALID_DATA_MARKER};
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

    [[nodiscard]] auto GetRegistrator() const noexcept -> const PropertyRegistrator* { return _registrator; }
    [[nodiscard]] auto GetEntity() noexcept -> Entity* { FO_NON_CONST_METHOD_HINT_ONELINE() return _entity; }
    [[nodiscard]] auto Copy() const noexcept -> Properties;
    [[nodiscard]] auto GetRawData(const Property* prop) const noexcept -> const_span<uint8>;
    [[nodiscard]] auto GetRawDataSize(const Property* prop) const noexcept -> size_t;
    [[nodiscard]] auto GetPlainDataValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetPlainDataValueAsAny(const Property* prop) const -> any_t;
    [[nodiscard]] auto GetValueAsInt(int property_index) const -> int;
    [[nodiscard]] auto GetValueAsAny(int property_index) const -> any_t;
    [[nodiscard]] auto SavePropertyToText(const Property* prop) const -> string;
    [[nodiscard]] auto SaveToText(const Properties* base) const -> map<string, string>;

    void AllocData() noexcept;
    void SetEntity(Entity* entity) noexcept { _entity = entity; }
    void CopyFrom(const Properties& other) noexcept;
    void ValidateForRawData(const Property* prop) const noexcept(false);
    void ApplyFromText(const map<string, string>& key_values);
    void ApplyPropertyFromText(const Property* prop, string_view text);
    void StoreAllData(vector<uint8>& all_data, set<hstring>& str_hashes) const;
    void RestoreAllData(const vector<uint8>& all_data);
    void StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint32>** all_data_sizes) const;
    void RestoreData(const vector<const uint8*>& all_data, const vector<uint32>& all_data_sizes);
    void RestoreData(const vector<vector<uint8>>& all_data);
    void SetRawData(const Property* prop, const_span<uint8> raw_data) noexcept;
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetPlainDataValueAsInt(const Property* prop, int value);
    void SetPlainDataValueAsAny(const Property* prop, const any_t& value);
    void SetValueAsInt(int property_index, int value);
    void SetValueAsAny(int property_index, const any_t& value);
    void SetValueAsIntProps(int property_index, int value);
    void SetValueAsAnyProps(int property_index, const any_t& value);
    auto ResolveHash(hstring::hash_t h) const -> hstring;
    auto ResolveHash(hstring::hash_t h, bool* failed) const noexcept -> hstring;

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_pod_type_v<T> || is_strong_type_v<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));
        FO_RUNTIME_ASSERT(prop->IsPlainData());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);

            if (prop->_getter) {
                PropertyRawData prop_data = prop->_getter(_entity, prop);
                T result = prop_data.GetAs<T>();
                return result;
            }
            else {
                return {};
            }
        }

        FO_RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        auto result = *reinterpret_cast<const T*>(&_podData[prop->_podDataOffset]);
        return result;
    }

    template<typename T>
        requires(std::is_same_v<T, hstring>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
        FO_RUNTIME_ASSERT(prop->IsPlainData());
        FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);

            if (prop->_getter) {
                PropertyRawData prop_data = prop->_getter(_entity, prop);
                const auto hash = prop_data.GetAs<hstring::hash_t>();
                auto result = ResolveHash(hash);
                return result;
            }
            else {
                return {};
            }
        }

        FO_RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(&_podData[prop->_podDataOffset]);
        auto result = ResolveHash(hash);
        return result;
    }

    template<typename T>
        requires(std::is_same_v<T, string> || std::is_same_v<T, any_t>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->IsString());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);

            if (prop->_getter) {
                PropertyRawData prop_data = prop->_getter(_entity, prop);
                auto result = string(prop_data.GetPtrAs<char>(), prop_data.GetSize());
                return result;
            }
            else {
                return {};
            }
        }

        FO_RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        auto result = string(reinterpret_cast<const char*>(complex_data.first.get()), complex_data.second);
        return result;
    }

    template<typename T>
        requires(is_vector_v<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->IsArray());

        PropertyRawData prop_data;

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);

            if (prop->_getter) {
                prop_data = prop->_getter(_entity, prop);
            }
        }
        else {
            FO_RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
            const auto& complex_data = _complexData[prop->_complexDataIndex];
            prop_data.Pass({complex_data.first.get(), complex_data.second});
        }

        const auto* data = prop_data.GetPtrAs<uint8>();
        const auto data_size = prop_data.GetSize();

        T result;

        if (data_size != 0) {
            if constexpr (std::is_same_v<T, vector<string>> || std::is_same_v<T, vector<any_t>>) {
                FO_RUNTIME_ASSERT(prop->IsArrayOfString());

                uint32 arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    uint32 str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    if constexpr (std::is_same_v<T, vector<string>>) {
                        result[i] = string(reinterpret_cast<const char*>(data), str_size);
                    }
                    else {
                        result[i] = any_t {string(reinterpret_cast<const char*>(data), str_size)};
                    }

                    data += str_size;
                }
            }
            else if constexpr (std::is_same_v<T, vector<hstring>>) {
                FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());
                FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

                const auto arr_size = data_size / sizeof(hstring::hash_t);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    const auto hvalue = ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data));
                    result[i] = hvalue;
                    data += sizeof(hstring::hash_t);
                }
            }
            else {
                FO_RUNTIME_ASSERT(data_size % prop->GetBaseSize() == 0);
                result.resize(data_size / prop->GetBaseSize());
                MemCopy(result.data(), data, data_size);
            }
        }

        return result;
    }

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_pod_type_v<T> || is_strong_type_v<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(!prop->IsDisabled());
        FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(T));
        FO_STRONG_ASSERT(prop->IsPlainData());
        FO_STRONG_ASSERT(!prop->IsVirtual());

        FO_STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        auto result = *reinterpret_cast<const T*>(&_podData[prop->_podDataOffset]);
        return result;
    }

    template<typename T>
        requires(std::is_same_v<T, hstring>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(!prop->IsDisabled());
        FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
        FO_STRONG_ASSERT(prop->IsPlainData());
        FO_STRONG_ASSERT(prop->IsBaseTypeHash());
        FO_STRONG_ASSERT(!prop->IsVirtual());

        FO_STRONG_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(&_podData[prop->_podDataOffset]);
        auto result = ResolveHash(hash, nullptr);
        return result;
    }

    template<typename T>
        requires(std::is_same_v<T, string> || std::is_same_v<T, any_t>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> string_view
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(!prop->IsDisabled());
        FO_STRONG_ASSERT(prop->IsString());
        FO_STRONG_ASSERT(!prop->IsVirtual());

        FO_STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        const auto result = string_view(reinterpret_cast<const char*>(complex_data.first.get()), complex_data.second);
        return result;
    }

    template<typename T>
        requires(is_vector_v<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const noexcept -> T
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_STRONG_ASSERT(!prop->IsDisabled());
        FO_STRONG_ASSERT(prop->IsArray());
        FO_STRONG_ASSERT(!prop->IsVirtual());

        PropertyRawData prop_data;

        FO_STRONG_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);
        const auto& complex_data = _complexData[prop->_complexDataIndex];
        prop_data.Pass({complex_data.first.get(), complex_data.second});

        const auto* data = prop_data.GetPtrAs<uint8>();
        const auto data_size = prop_data.GetSize();

        T result;

        if (data_size != 0) {
            if constexpr (std::is_same_v<T, vector<string>> || std::is_same_v<T, vector<any_t>>) {
                FO_STRONG_ASSERT(prop->IsArrayOfString());

                uint32 arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    uint32 str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    if constexpr (std::is_same_v<T, vector<string>>) {
                        result[i] = string(reinterpret_cast<const char*>(data), str_size);
                    }
                    else {
                        result[i] = any_t {string(reinterpret_cast<const char*>(data), str_size)};
                    }

                    data += str_size;
                }
            }
            else if constexpr (std::is_same_v<T, vector<hstring>>) {
                FO_STRONG_ASSERT(prop->IsBaseTypeHash());
                FO_STRONG_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

                const auto arr_size = data_size / sizeof(hstring::hash_t);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    const auto hvalue = ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data), nullptr);
                    result[i] = hvalue;
                    data += sizeof(hstring::hash_t);
                }
            }
            else {
                FO_STRONG_ASSERT(data_size % prop->GetBaseSize() == 0);
                result.resize(data_size / prop->GetBaseSize());
                MemCopy(result.data(), data, data_size);
            }
        }

        return result;
    }

    template<typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T> || is_valid_pod_type_v<T> || is_strong_type_v<T>)
    void SetValue(const Property* prop, T new_value)
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));
        FO_RUNTIME_ASSERT(prop->IsPlainData());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);
            FO_RUNTIME_ASSERT(!prop->_setters.empty());

            PropertyRawData prop_data;
            prop_data.SetAs<T>(new_value);

            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            FO_RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);

            if (new_value != *reinterpret_cast<T*>(&_podData[prop->_podDataOffset])) {
                if (!prop->_setters.empty() && _entity != nullptr) {
                    PropertyRawData prop_data;
                    prop_data.SetAs<T>(new_value);

                    for (const auto& setter : prop->_setters) {
                        setter(_entity, prop, prop_data);
                    }

                    *reinterpret_cast<T*>(&_podData[prop->_podDataOffset]) = prop_data.GetAs<T>();
                }
                else {
                    *reinterpret_cast<T*>(&_podData[prop->_podDataOffset]) = new_value;
                }

                if (_entity != nullptr) {
                    for (const auto& setter : prop->_postSetters) {
                        setter(_entity, prop);
                    }
                }
            }
        }
    }

    template<typename T>
        requires(std::is_same_v<T, hstring>)
    void SetValue(const Property* prop, T new_value)
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
        FO_RUNTIME_ASSERT(prop->IsPlainData());
        FO_RUNTIME_ASSERT(prop->IsBaseTypeHash());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);
            FO_RUNTIME_ASSERT(!prop->_setters.empty());

            PropertyRawData prop_data;
            prop_data.SetAs<hstring::hash_t>(new_value.as_hash());

            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            FO_RUNTIME_ASSERT(prop->_podDataOffset != Property::INVALID_DATA_MARKER);
            const auto new_value_hash = new_value.as_hash();

            if (new_value_hash != *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset])) {
                if (!prop->_setters.empty() && _entity != nullptr) {
                    PropertyRawData prop_data;
                    prop_data.SetAs<hstring::hash_t>(new_value_hash);

                    for (const auto& setter : prop->_setters) {
                        setter(_entity, prop, prop_data);
                    }

                    *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset]) = prop_data.GetAs<hstring::hash_t>();
                }
                else {
                    *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset]) = new_value_hash;
                }

                if (_entity != nullptr) {
                    for (const auto& setter : prop->_postSetters) {
                        setter(_entity, prop);
                    }
                }
            }
        }
    }

    template<typename T>
        requires(std::is_same_v<T, string> || std::is_same_v<T, any_t>)
    void SetValue(const Property* prop, const T& new_value)
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->IsString());

        if (prop->IsVirtual()) {
            FO_RUNTIME_ASSERT(_entity);
            FO_RUNTIME_ASSERT(!prop->_setters.empty());

            PropertyRawData prop_data;
            prop_data.Pass(new_value.c_str(), new_value.length());

            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            FO_RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

            if (!prop->_setters.empty() && _entity != nullptr) {
                PropertyRawData prop_data;
                prop_data.Pass(new_value.c_str(), new_value.length());

                for (const auto& setter : prop->_setters) {
                    setter(_entity, prop, prop_data);
                }

                SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});
            }
            else {
                SetRawData(prop, {reinterpret_cast<const uint8*>(new_value.c_str()), new_value.length()});
            }

            if (_entity != nullptr) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity, prop);
                }
            }
        }
    }

    template<typename T>
    void SetValue(const Property* prop, const vector<T>& new_value)
    {
        FO_NO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(!prop->IsDisabled());
        FO_RUNTIME_ASSERT(prop->IsArray());

        PropertyRawData prop_data;

        if constexpr (std::is_same_v<T, string> || std::is_same_v<T, any_t>) {
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
        else if constexpr (std::is_same_v<T, hstring>) {
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
                setter(_entity, prop, prop_data);
            }
        }
        else {
            FO_RUNTIME_ASSERT(prop->_complexDataIndex != Property::INVALID_DATA_MARKER);

            if (!prop->_setters.empty() && _entity != nullptr) {
                for (const auto& setter : prop->_setters) {
                    setter(_entity, prop, prop_data);
                }
            }

            SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

            if (_entity != nullptr) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity, prop);
                }
            }
        }
    }

    template<typename T>
        requires(is_map_v<T>)
    [[nodiscard]] auto GetValue(const Property* prop) const -> T = delete;
    template<typename T>
        requires(is_map_v<T>)
    [[nodiscard]] auto GetValueFast(const Property* prop) const -> T = delete;
    template<typename T, typename U>
    void SetValue(const Property* prop, const map<T, U>& new_value) = delete;

private:
    const PropertyRegistrator* _registrator;
    unique_ptr<uint8[]> _podData {};
    unique_ptr<pair<unique_ptr<uint8[]>, size_t>[]> _complexData {};

    mutable unique_ptr<vector<const uint8*>> _storeData {};
    mutable unique_ptr<vector<uint32>> _storeDataSizes {};
    mutable unique_ptr<vector<uint16>> _storeDataComplexIndices {};
    Entity* _entity {};
    bool _nonConstHelper {};
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
    [[nodiscard]] auto GetPropertiesCount() const noexcept -> size_t { return _registeredProperties.size(); }
    [[nodiscard]] auto FindProperty(string_view property_name, bool* is_component = nullptr) const -> const Property*;
    [[nodiscard]] auto GetPropertyByIndex(int property_index) const noexcept -> const Property*;
    [[nodiscard]] auto GetPropertyByIndexUnsafe(size_t property_index) const noexcept -> const Property* { return _registeredProperties[property_index].get(); }
    [[nodiscard]] auto IsComponentRegistered(hstring component_name) const noexcept -> bool;
    [[nodiscard]] auto GetWholeDataSize() const noexcept -> size_t { return _wholePodDataSize; }
    [[nodiscard]] auto GetProperties() const noexcept -> const vector<const Property*>& { return _constRegisteredProperties; }
    [[nodiscard]] auto GetPropertyGroups() const noexcept -> const map<string, vector<const Property*>>& { return _propertyGroups; }
    [[nodiscard]] auto GetComponents() const noexcept -> const unordered_set<hstring>& { return _registeredComponents; }
    [[nodiscard]] auto GetHashResolver() const noexcept -> const HashResolver& { return _hashResolver; }
    [[nodiscard]] auto GetNameResolver() const noexcept -> const NameResolver& { return _nameResolver; }

    void RegisterComponent(string_view name);
    void RegisterProperty(const initializer_list<string_view>& flags) { RegisterProperty({flags.begin(), flags.end()}); }
    void RegisterProperty(const const_span<string_view>& flags);

private:
    const hstring _typeName;
    const hstring _typeNamePlural;
    const PropertiesRelationType _relation;
    const hstring _propMigrationRuleName;
    const hstring _componentMigrationRuleName;
    HashResolver& _hashResolver;
    NameResolver& _nameResolver;
    vector<unique_ptr<Property>> _registeredProperties {};
    vector<const Property*> _constRegisteredProperties {};
    unordered_map<string, const Property*> _registeredPropertiesLookup {};
    unordered_set<hstring> _registeredComponents {};
    map<string, vector<const Property*>> _propertyGroups {};
    unordered_map<string_view, Property::AccessType> _accessMap {};

    // PlainData info
    size_t _wholePodDataSize {};
    vector<bool> _publicPodDataSpace {};
    vector<bool> _protectedPodDataSpace {};
    vector<bool> _privatePodDataSpace {};

    // Complex types info
    vector<Property*> _complexProperties {};
    vector<uint16> _publicComplexDataProps {};
    vector<uint16> _protectedComplexDataProps {};
    vector<uint16> _publicProtectedComplexDataProps {};
};

FO_END_NAMESPACE();
