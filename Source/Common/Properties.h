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

// Todo: don't preserve memory for not allocated components in entity
// Todo: pack bool properties to one bit

#pragma once

#include "Common.h"

DECLARE_EXCEPTION(PropertyRegistrationException);
DECLARE_EXCEPTION(PropertiesException);

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

    [[nodiscard]] auto GetPtr() -> void*;
    [[nodiscard]] auto GetSize() const -> uint;
    [[nodiscard]] auto Alloc(size_t size) -> uchar*;

    template<typename T>
    [[nodiscard]] auto GetPtrAs() -> T*
    {
        return static_cast<T*>(GetPtr());
    }

    template<typename T>
    [[nodiscard]] auto GetAs() -> T
    {
        RUNTIME_ASSERT(sizeof(T) == _dataSize);
        return *static_cast<T*>(GetPtr());
    }

    template<typename T>
    void SetAs(T value)
    {
        std::memcpy(Alloc(sizeof(T)), &value, sizeof(T));
    }

    void Set(const void* value, size_t size) { std::memcpy(Alloc(size), value, size); }

    void Pass(const void* value, size_t size);
    void StoreIfPassed();

private:
    size_t _dataSize {};
    bool _useDynamic {};
    uchar _localBuf[LOCAL_BUF_SIZE] {};
    unique_ptr<uchar[]> _dynamicBuf {};
    void* _passedPtr {};
};

using PropertyGetCallback = std::function<PropertyRawData(Entity*, const Property*)>;
using PropertySetCallback = std::function<void(Entity*, const Property*, PropertyRawData&)>;
using PropertyPostSetCallback = std::function<void(Entity*, const Property*)>;

class Property final
{
    friend class PropertyRegistrator;
    friend class Properties;
    friend class PropertiesSerializator; // Todo: remove friend from PropertiesSerializator and use public Property interface

public:
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
    auto operator=(const Property&) = delete;
    auto operator=(Property&&) noexcept = delete;
    ~Property() = default;

    [[nodiscard]] auto GetRegistrator() const -> const PropertyRegistrator* { return _registrator; }
    [[nodiscard]] auto GetName() const -> const string& { return _propName; }
    [[nodiscard]] auto GetNameWithoutComponent() const -> const string& { return _propNameWithoutComponent; }
    [[nodiscard]] auto GetFullTypeName() const -> const string& { return _fullTypeName; }
    [[nodiscard]] auto GetBaseTypeName() const -> const string& { return _baseTypeName; }
    [[nodiscard]] auto GetDictKeyTypeName() const -> const string& { return _dictKeyTypeName; }
    [[nodiscard]] auto GetComponent() const -> const hstring& { return _component; }

    [[nodiscard]] auto GetRegIndex() const -> ushort { return _regIndex; }
    [[nodiscard]] auto GetAccess() const -> AccessType { return _accessType; }
    [[nodiscard]] auto GetBaseSize() const -> uint { return _baseSize; }
    [[nodiscard]] auto GetDictKeySize() const -> uint { return _dictKeySize; }
    [[nodiscard]] auto IsPlainData() const -> bool { return _dataType == DataType::PlainData; }
    [[nodiscard]] auto IsString() const -> bool { return _dataType == DataType::String; }
    [[nodiscard]] auto IsArray() const -> bool { return _dataType == DataType::Array; }
    [[nodiscard]] auto IsArrayOfString() const -> bool { return _isArrayOfString; }
    [[nodiscard]] auto IsDict() const -> bool { return _dataType == DataType::Dict; }
    [[nodiscard]] auto IsDictOfString() const -> bool { return _isDictOfString; }
    [[nodiscard]] auto IsDictOfArray() const -> bool { return _isDictOfArray; }
    [[nodiscard]] auto IsDictOfArrayOfString() const -> bool { return _isDictOfArrayOfString; }
    [[nodiscard]] auto IsDictKeyHash() const -> bool { return _isDictKeyHash; }
    [[nodiscard]] auto IsDictKeyEnum() const -> bool { return _isDictKeyEnum; }

    [[nodiscard]] auto IsInt() const -> bool { return _isInt; }
    [[nodiscard]] auto IsSignedInt() const -> bool { return _isSignedInt; }
    [[nodiscard]] auto IsFloat() const -> bool { return _isFloat; }
    [[nodiscard]] auto IsBool() const -> bool { return _isBool; }

    [[nodiscard]] auto IsBaseTypeHash() const -> bool { return _isHashBase; }
    [[nodiscard]] auto IsBaseTypeResource() const -> bool { return _isResourceHash; }
    [[nodiscard]] auto IsBaseTypeEnum() const -> bool { return _isEnumBase; }
    [[nodiscard]] auto IsBaseScriptFuncType() const -> bool { return !_scriptFuncType.empty(); }
    [[nodiscard]] auto GetBaseScriptFuncType() const -> const string& { return _scriptFuncType; }

    [[nodiscard]] auto IsDisabled() const -> bool { return _isDisabled; }
    [[nodiscard]] auto IsVirtual() const -> bool { return _isVirtual; }
    [[nodiscard]] auto IsReadOnly() const -> bool { return _isReadOnly; }
    [[nodiscard]] auto IsTemporary() const -> bool { return _isTemporary; }
    [[nodiscard]] auto IsHistorical() const -> bool { return _isHistorical; }
    [[nodiscard]] auto IsNullGetterForProto() const -> bool { return _isNullGetterForProto; }

    [[nodiscard]] auto GetGetter() const -> auto& { return _getter; }
    [[nodiscard]] auto GetSetters() const -> auto& { return _setters; }
    [[nodiscard]] auto GetPostSetters() const -> auto& { return _postSetters; }

    void SetGetter(PropertyGetCallback getter) const;
    void AddSetter(PropertySetCallback setter) const;
    void AddPostSetter(PropertyPostSetCallback setter) const;

private:
    enum class DataType
    {
        PlainData,
        String,
        Array,
        Dict,
    };

    explicit Property(const PropertyRegistrator* registrator);

    const PropertyRegistrator* _registrator;

    mutable PropertyGetCallback _getter {};
    mutable vector<PropertySetCallback> _setters {};
    mutable vector<PropertyPostSetCallback> _postSetters {};

    string _propName {};
    vector<string> _propNameAliases {};
    string _propNameWithoutComponent {};
    string _fullTypeName {};
    hstring _component {};
    DataType _dataType {};

    bool _isStringBase {};
    bool _isHashBase {};
    bool _isResourceHash {};
    bool _isInt {};
    bool _isSignedInt {};
    bool _isFloat {};
    bool _isBool {};
    bool _isEnumBase {};
    uint _baseSize {};
    string _baseTypeName {};
    string _scriptFuncType {};

    bool _isInt8 {};
    bool _isInt16 {};
    bool _isInt32 {};
    bool _isInt64 {};
    bool _isUInt8 {};
    bool _isUInt16 {};
    bool _isUInt32 {};
    bool _isUInt64 {};
    bool _isSingleFloat {};
    bool _isDoubleFloat {};

    bool _isArrayOfString {};

    bool _isDictOfString {};
    bool _isDictOfArray {};
    bool _isDictOfArrayOfString {};
    bool _isDictKeyHash {};
    bool _isDictKeyEnum {};
    uint _dictKeySize {};
    string _dictKeyTypeName {};

    AccessType _accessType {};
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
    ushort _regIndex {};
    uint _podDataOffset {};
    uint _complexDataIndex {};
};

class Properties final
{
    friend class PropertyRegistrator;
    friend class Property;
    friend class PropertiesSerializator;

public:
    Properties() = delete;
    explicit Properties(const PropertyRegistrator* registrator);
    Properties(const Properties& other);
    Properties(Properties&&) noexcept = default;
    auto operator=(const Properties& other) -> Properties&;
    auto operator=(Properties&&) noexcept = delete;
    ~Properties();

    [[nodiscard]] auto GetRegistrator() const -> const PropertyRegistrator* { return _registrator; }
    [[nodiscard]] auto GetEntity() -> Entity* { NON_CONST_METHOD_HINT_ONELINE() return _entity; }
    [[nodiscard]] auto GetRawDataSize(const Property* prop) const -> uint;
    [[nodiscard]] auto GetRawData(const Property* prop, uint& data_size) const -> const uchar*;
    [[nodiscard]] auto GetRawData(const Property* prop, uint& data_size) -> uchar*;
    [[nodiscard]] auto GetPlainDataValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetPlainDataValueAsFloat(const Property* prop) const -> float;
    [[nodiscard]] auto GetValueAsInt(int property_index) const -> int;
    [[nodiscard]] auto GetValueAsFloat(int property_index) const -> float;
    [[nodiscard]] auto SavePropertyToText(const Property* prop) const -> string;
    [[nodiscard]] auto SaveToText(const Properties* base) const -> map<string, string>;

    void AllocData();
    void SetEntity(Entity* entity) { _entity = entity; }
    auto LoadFromText(const map<string, string>& key_values) -> bool;
    auto LoadPropertyFromText(const Property* prop, string_view text) -> bool;
    void StoreAllData(vector<uchar>& all_data) const;
    void RestoreAllData(const vector<uchar>& all_data);
    auto StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint;
    void RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes);
    void RestoreData(const vector<vector<uchar>>& all_data);
    void SetRawData(const Property* prop, const uchar* data, uint data_size);
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetPlainDataValueAsInt(const Property* prop, int value);
    void SetPlainDataValueAsFloat(const Property* prop, float value);
    void SetValueAsInt(int property_index, int value);
    void SetValueAsFloat(int property_index, float value);
    void SetValueAsIntProps(int property_index, int value);
    auto ResolveHash(hstring::hash_t h) const -> hstring;

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->IsPlainData());

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            if (prop->_getter) {
                PropertyRawData prop_data = prop->_getter(_entity, prop);
                T result = prop_data.GetAs<T>();
                return result;
            }
            else {
                return {};
            }
        }

        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        auto result = *reinterpret_cast<T*>(&_podData[prop->_podDataOffset]);
        return result;
    }

    template<typename T, std::enable_if_t<is_strong_type<T>::value, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        return T {GetValue<typename T::underlying_type>(prop)};
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, hstring>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(sizeof(hstring::hash_t) == prop->_baseSize);
        RUNTIME_ASSERT(prop->IsPlainData());
        RUNTIME_ASSERT(prop->IsBaseTypeHash());

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
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

        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        const auto hash = *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset]);
        auto result = ResolveHash(hash);
        return result;
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            if (prop->_getter) {
                PropertyRawData prop_data = prop->_getter(_entity, prop);
                auto result = string(prop_data.GetPtrAs<char>(), prop_data.GetSize());
                return result;
            }
            else {
                return {};
            }
        }

        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        const auto data_size = _complexDataSizes[prop->_complexDataIndex];
        auto result = data_size != 0u ? string(reinterpret_cast<char*>(_complexData[prop->_complexDataIndex]), data_size) : string();
        return result;
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);

        PropertyRawData prop_data;

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            if (prop->_getter) {
                prop_data = prop->_getter(_entity, prop);
            }
        }
        else {
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            prop_data.Pass(_complexData[prop->_complexDataIndex], _complexDataSizes[prop->_complexDataIndex]);
        }

        const auto* data = prop_data.GetPtrAs<uchar>();
        const auto data_size = prop_data.GetSize();

        T result;

        if (data_size != 0u) {
            if constexpr (std::is_same_v<T, vector<string>>) {
                RUNTIME_ASSERT(prop->IsArrayOfString());

                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);
                    result[i] = string(reinterpret_cast<const char*>(data), str_size);
                    data += str_size;
                }
            }
            else if constexpr (std::is_same_v<T, vector<hstring>>) {
                RUNTIME_ASSERT(prop->IsBaseTypeHash());
                RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

                const auto arr_size = data_size / sizeof(hstring::hash_t);
                result.resize(arr_size);

                for (const auto i : xrange(arr_size)) {
                    const auto hvalue = ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data));
                    result[i] = hvalue;
                    data += sizeof(hstring::hash_t);
                }
            }
            else {
                RUNTIME_ASSERT(data_size % prop->GetBaseSize() == 0);
                result.resize(data_size / prop->GetBaseSize());
                std::memcpy(result.data(), data, data_size);
            }
        }

        return result;
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->IsPlainData());

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            RUNTIME_ASSERT(!prop->_setters.empty());
            PropertyRawData prop_data;
            prop_data.SetAs<T>(new_value);
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
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

    template<typename T, std::enable_if_t<is_strong_type<T>::value, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        SetValue<typename T::underlying_type>(prop, new_value.underlying_value());
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, hstring>, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(sizeof(hstring::hash_t) == prop->_baseSize);
        RUNTIME_ASSERT(prop->IsPlainData());
        RUNTIME_ASSERT(prop->IsBaseTypeHash());

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            RUNTIME_ASSERT(!prop->_setters.empty());
            PropertyRawData prop_data;
            prop_data.SetAs<hstring::hash_t>(new_value.as_hash());
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
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

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    void SetValue(const Property* prop, const T& new_value)
    {
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            RUNTIME_ASSERT(!prop->_setters.empty());
            PropertyRawData prop_data;
            prop_data.Pass(new_value.c_str(), new_value.length());
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            if (!prop->_setters.empty() && _entity != nullptr) {
                PropertyRawData prop_data;
                prop_data.Pass(new_value.c_str(), new_value.length());
                for (const auto& setter : prop->_setters) {
                    setter(_entity, prop, prop_data);
                }

                SetRawData(prop, prop_data.GetPtrAs<uchar>(), prop_data.GetSize());
            }
            else {
                SetRawData(prop, reinterpret_cast<const uchar*>(new_value.c_str()), static_cast<uint>(new_value.length()));
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
        RUNTIME_ASSERT(!prop->IsDisabled());
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);

        PropertyRawData prop_data;

        if constexpr (std::is_same_v<T, string>) {
            if (!new_value.empty()) {
                size_t data_size = sizeof(uint);
                for (const auto& str : new_value) {
                    data_size += sizeof(uint) + static_cast<uint>(str.length());
                }

                auto* buf = prop_data.Alloc(data_size);

                const auto arr_size = static_cast<uint>(new_value.size());
                std::memcpy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint);

                for (const auto& str : new_value) {
                    const auto str_size = static_cast<uint>(str.length());
                    std::memcpy(buf, &str_size, sizeof(str_size));
                    buf += sizeof(str_size);

                    if (str_size != 0u) {
                        std::memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<T, hstring>) {
            RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));
            if (!new_value.empty()) {
                auto* buf = prop_data.Alloc(new_value.size() * sizeof(hstring::hash_t));
                for (const auto& hstr : new_value) {
                    const auto hash = hstr.as_hash();
                    std::memcpy(buf, &hash, sizeof(hstring::hash_t));
                    buf += sizeof(hstring::hash_t);
                }
            }
        }
        else {
            RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(T));
            if (!new_value.empty()) {
                prop_data.Pass(new_value.data(), new_value.size() * sizeof(T));
            }
        }

        if (prop->IsVirtual()) {
            RUNTIME_ASSERT(_entity);
            RUNTIME_ASSERT(!prop->_setters.empty());
            for (const auto& setter : prop->_setters) {
                setter(_entity, prop, prop_data);
            }
        }
        else {
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            if (!prop->_setters.empty() && _entity != nullptr) {
                for (const auto& setter : prop->_setters) {
                    setter(_entity, prop, prop_data);
                }
            }

            SetRawData(prop, prop_data.GetPtrAs<uchar>(), prop_data.GetSize());

            if (_entity != nullptr) {
                for (const auto& setter : prop->_postSetters) {
                    setter(_entity, prop);
                }
            }
        }
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T = delete;
    template<typename T, typename U>
    void SetValue(const Property* prop, const map<T, U>& new_value) = delete;

private:
    const PropertyRegistrator* _registrator;
    uchar* _podData {};
    vector<uchar*> _complexData {};
    vector<uint> _complexDataSizes {};
    mutable vector<uchar*> _storeData {};
    mutable vector<uint> _storeDataSizes {};
    mutable vector<ushort> _storeDataComplexIndices {};
    Entity* _entity {};
    bool _nonConstHelper {};
};

class PropertyRegistrator final
{
    friend class Properties;
    friend class Property;
    friend class PropertiesSerializator;

public:
    PropertyRegistrator() = delete;
    PropertyRegistrator(string_view class_name, PropertiesRelationType relation, NameResolver& name_resolver);
    PropertyRegistrator(const PropertyRegistrator&) = delete;
    PropertyRegistrator(PropertyRegistrator&&) noexcept = default;
    auto operator=(const PropertyRegistrator&) = delete;
    auto operator=(PropertyRegistrator&&) noexcept = delete;
    ~PropertyRegistrator();

    [[nodiscard]] auto GetClassName() const -> const string&;
    [[nodiscard]] auto GetCount() const -> uint;
    [[nodiscard]] auto Find(string_view property_name) const -> const Property*;
    [[nodiscard]] auto GetByIndex(int property_index) const -> const Property*;
    [[nodiscard]] auto GetByIndexFast(size_t property_index) const -> const Property* { return _registeredProperties[property_index]; }
    [[nodiscard]] auto IsComponentRegistered(hstring component_name) const -> bool;
    [[nodiscard]] auto GetWholeDataSize() const -> uint;
    [[nodiscard]] auto GetPropertyGroups() const -> const map<string, vector<const Property*>>&;
    [[nodiscard]] auto GetComponents() const -> const unordered_set<hstring>&;
    [[nodiscard]] auto GetNameResolver() const -> const NameResolver& { return _nameResolver; }

    void RegisterComponent(string_view name);

private:
    const string _className;
    const PropertiesRelationType _relation;
    NameResolver& _nameResolver;
    vector<Property*> _registeredProperties {};
    unordered_map<string, const Property*> _registeredPropertiesLookup {};
    unordered_set<hstring> _registeredComponents {};
    map<string, vector<const Property*>> _propertyGroups {};

    // PlainData info
    uint _wholePodDataSize {};
    vector<bool> _publicPodDataSpace {};
    vector<bool> _protectedPodDataSpace {};
    vector<bool> _privatePodDataSpace {};
    mutable vector<uchar*> _podDataPool {};

    // Complex types info
    vector<Property*> _complexProperties {};
    vector<ushort> _publicComplexDataProps {};
    vector<ushort> _protectedComplexDataProps {};
    vector<ushort> _publicProtectedComplexDataProps {};

public:
    template<typename T>
    void Register(Property::AccessType access, string_view name, const vector<string_view>& flags)
    {
        RegisterEx<T>(access, name, flags);
    }

    template<typename T>
    void Register(Property::AccessType access, string_view name, const initializer_list<string_view>& flags)
    {
        RegisterEx<T>(access, name, {flags.begin(), flags.end()});
    }

    template<typename T>
    void RegisterEx(Property::AccessType access, string_view name, const const_span<string_view>& flags)
    {
        auto* prop = new Property(this);

        prop->_propName = name;
        prop->_accessType = access;
        prop->_dataType = TypeInfo<T>::DATA_TYPE;

        using base_type = typename TypeInfo<T>::BaseType::Type;

        if constexpr (!std::is_same_v<base_type, string>) {
            static_assert(sizeof(base_type) == 1u || sizeof(base_type) == 2u || sizeof(base_type) == 4u || sizeof(base_type) == 8u);
            prop->_baseSize = sizeof(base_type);
            prop->_isHashBase = TypeInfo<T>::BaseType::IS_HASH;
            prop->_isEnumBase = TypeInfo<T>::BaseType::IS_ENUM;

            prop->_isInt = (std::is_integral_v<base_type> && !std::is_same_v<base_type, bool>);
            prop->_isSignedInt = (prop->_isInt && std::is_signed_v<base_type>);
            prop->_isFloat = std::is_floating_point_v<base_type>;
            prop->_isBool = std::is_same_v<base_type, bool>;

            prop->_isInt8 = (prop->_isInt && prop->_isSignedInt && prop->_baseSize == 1u);
            prop->_isInt16 = (prop->_isInt && prop->_isSignedInt && prop->_baseSize == 2u);
            prop->_isInt32 = (prop->_isInt && prop->_isSignedInt && prop->_baseSize == 4u);
            prop->_isInt64 = (prop->_isInt && prop->_isSignedInt && prop->_baseSize == 8u);
            prop->_isUInt8 = (prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 1u);
            prop->_isUInt16 = (prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 2u);
            prop->_isUInt32 = (prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 4u);
            prop->_isUInt64 = (prop->_isInt && !prop->_isSignedInt && prop->_baseSize == 8u);

            prop->_isSingleFloat = (prop->_isFloat && prop->_baseSize == 4u);
            prop->_isDoubleFloat = (prop->_isFloat && prop->_baseSize == 8u);

            if (prop->_isHashBase) {
                prop->_baseTypeName = "hstring";
            }
            else if (prop->_isInt) {
                prop->_baseTypeName = "int";
                if (prop->_baseSize == 1u) {
                    prop->_baseTypeName += "8";
                }
                else if (prop->_baseSize == 2u) {
                    prop->_baseTypeName += "16";
                }
                else if (prop->_baseSize == 8u) {
                    prop->_baseTypeName += "64";
                }
                if (!prop->_isSignedInt) {
                    prop->_baseTypeName = "u" + prop->_baseTypeName;
                }
            }
            else if (prop->_isFloat) {
                if (prop->_isSingleFloat) {
                    prop->_baseTypeName = "float";
                }
                else if (prop->_isDoubleFloat) {
                    prop->_baseTypeName = "double";
                }
            }
            else if (prop->_isBool) {
                prop->_baseTypeName = "bool";
            }
        }
        else {
            prop->_baseTypeName = "string";
        }

        if constexpr (is_specialization<T, vector>::value) {
            prop->_isArrayOfString = std::is_same_v<base_type, string>;
        }

        if constexpr (is_specialization<T, map>::value) {
            using key_type = typename TypeInfo<T>::KeyType::Type;
            static_assert(std::is_integral_v<key_type>);
            static_assert(!std::is_same_v<key_type, bool>);
            static_assert(sizeof(key_type) == 1u || sizeof(key_type) == 2u || sizeof(key_type) == 4u || sizeof(key_type) == 8u);
            prop->_isDictOfArray = TypeInfo<T>::IS_DICT_OF_ARRAY;
            prop->_isDictOfString = !prop->_isDictOfArray && std::is_same_v<base_type, string>;
            prop->_isDictOfArrayOfString = prop->_isDictOfArray && std::is_same_v<base_type, string>;
            prop->_dictKeySize = sizeof(key_type);
            prop->_isDictKeyHash = TypeInfo<T>::KeyType::IS_HASH;
            prop->_isDictKeyEnum = TypeInfo<T>::KeyType::IS_ENUM;

            if (prop->_isDictKeyHash) {
                prop->_dictKeyTypeName = "hstring";
            }
            else {
                prop->_dictKeyTypeName = "int";
                if (sizeof(key_type) == 1u) {
                    prop->_dictKeyTypeName += "8";
                }
                else if (sizeof(key_type) == 2u) {
                    prop->_dictKeyTypeName += "16";
                }
                else if (sizeof(key_type) == 8u) {
                    prop->_dictKeyTypeName += "64";
                }
                if (std::is_unsigned_v<key_type>) {
                    prop->_dictKeyTypeName = "u" + prop->_dictKeyTypeName;
                }
            }
        }

        prop->_isStringBase = prop->_dataType == Property::DataType::String || prop->_isArrayOfString || prop->_isDictOfString || prop->_isDictOfArrayOfString;

        AppendProperty(prop, {flags.begin(), flags.end()});
    }

private:
    template<typename T, typename Enable = void>
    struct BaseTypeInfo;

    template<typename T>
    struct BaseTypeInfo<T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_same_v<T, string>>>
    {
        using Type = T;
        static constexpr auto IS_HASH = false;
        static constexpr auto IS_ENUM = false;
    };

    template<typename T>
    struct BaseTypeInfo<T, std::enable_if_t<is_strong_type<T>::value>>
    {
        using Type = typename T::underlying_type;
        static constexpr auto IS_HASH = BaseTypeInfo<typename T::underlying_type>::IS_HASH;
        static constexpr auto IS_ENUM = BaseTypeInfo<typename T::underlying_type>::IS_ENUM;
    };

    template<typename T>
    struct BaseTypeInfo<T, std::enable_if_t<std::is_same_v<T, hstring>>>
    {
        using Type = hstring::hash_t;
        static constexpr auto IS_HASH = true;
        static constexpr auto IS_ENUM = false;
    };

    template<typename T>
    struct BaseTypeInfo<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        using Type = std::underlying_type_t<T>;
        static constexpr auto IS_HASH = false;
        static constexpr auto IS_ENUM = true;
        static_assert(std::is_same_v<Type, uchar> || std::is_same_v<Type, ushort> || std::is_same_v<Type, int> || std::is_same_v<Type, uint>);
    };

    template<typename T, typename Enable = void>
    struct TypeInfo;

    template<typename T>
    struct TypeInfo<T, std::enable_if_t<!std::is_same_v<T, string> && !is_specialization<T, vector>::value && !is_specialization<T, map>::value>>
    {
        using BaseType = BaseTypeInfo<T>;
        static constexpr auto DATA_TYPE = Property::DataType::PlainData;
    };

    template<typename T>
    struct TypeInfo<T, std::enable_if_t<std::is_same_v<T, string>>>
    {
        using BaseType = BaseTypeInfo<T>;
        static constexpr auto DATA_TYPE = Property::DataType::String;
    };

    template<typename T>
    struct TypeInfo<T, std::enable_if_t<is_specialization<T, vector>::value>>
    {
        using BaseType = BaseTypeInfo<typename T::value_type>;
        static constexpr auto DATA_TYPE = Property::DataType::Array;
    };

    template<typename T>
    struct TypeInfo<T, std::enable_if_t<is_specialization<T, map>::value && is_specialization<typename T::mapped_type, vector>::value>>
    {
        using BaseType = BaseTypeInfo<typename T::mapped_type::value_type>;
        using KeyType = BaseTypeInfo<typename T::key_type>;
        static constexpr auto DATA_TYPE = Property::DataType::Dict;
        static constexpr auto IS_DICT_OF_ARRAY = true;
    };

    template<typename T>
    struct TypeInfo<T, std::enable_if_t<is_specialization<T, map>::value && !is_specialization<typename T::mapped_type, vector>::value>>
    {
        using BaseType = BaseTypeInfo<typename T::mapped_type>;
        using KeyType = BaseTypeInfo<typename T::key_type>;
        static constexpr auto DATA_TYPE = Property::DataType::Dict;
        static constexpr auto IS_DICT_OF_ARRAY = false;
    };

    void AppendProperty(Property* prop, const const_span<string_view>& flags);
};
