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
using PropertyVec = vector<const Property*>;
using NativeSendCallback = std::function<void(Entity*, const Property*)>;
using NativeCallback = std::function<void(Entity*, const Property*, void*, void*)>;
using NativeCallbackVec = vector<NativeCallback>;

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
        PublicStatic = 0x0800,
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

    [[nodiscard]] auto GetName() const -> const string& { return _propName; }
    [[nodiscard]] auto GetFullTypeName() const -> const string& { return _fullTypeName; }
    [[nodiscard]] auto GetBaseTypeName() const -> const string& { return _baseTypeName; }
    [[nodiscard]] auto GetDictKeyTypeName() const -> const string& { return _dictKeyTypeName; }

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
    [[nodiscard]] auto IsHash() const -> bool { return _isHash; }
    [[nodiscard]] auto IsResource() const -> bool { return _isResourceHash; }
    [[nodiscard]] auto IsEnum() const -> bool { return _isEnum; }

    [[nodiscard]] auto IsReadable() const -> bool { return _isReadable; }
    [[nodiscard]] auto IsWritable() const -> bool { return _isWritable; }
    [[nodiscard]] auto IsReadOnly() const -> bool { return _isReadOnly; }
    [[nodiscard]] auto IsTemporary() const -> bool { return _isTemporary; }
    [[nodiscard]] auto IsHistorical() const -> bool { return _isHistorical; }

private:
    enum class DataType
    {
        PlainData,
        String,
        Array,
        Dict,
    };

    explicit Property(const PropertyRegistrator* registrator);
    static auto CreateRefValue(uchar* data, uint data_size) -> unique_ptr<void, std::function<void(void*)>>;
    static auto ExpandComplexValueData(void* pvalue, uint& data_size, bool& need_delete) -> uchar*;

    const PropertyRegistrator* _registrator;

    string _propName {};
    string _fullTypeName {};
    string _componentName {};
    DataType _dataType {};

    bool _isHash {};
    bool _isResourceHash {};
    bool _isInt {};
    bool _isSignedInt {};
    bool _isFloat {};
    bool _isBool {};
    bool _isEnum {};
    uint _baseSize {};
    string _baseTypeName {};

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
    bool _isReadOnly {};
    bool _isReadable {};
    bool _isWritable {};
    bool _checkMinValue {};
    bool _checkMaxValue {};
    int64 _minValue {};
    int64 _maxValue {};
    bool _isTemporary {};
    bool _isHistorical {};
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
    [[nodiscard]] auto GetRawDataSize(const Property* prop) const -> uint;
    [[nodiscard]] auto GetRawData(const Property* prop, uint& data_size) const -> const uchar*;
    [[nodiscard]] auto GetRawData(const Property* prop, uint& data_size) -> uchar*;
    [[nodiscard]] auto GetPlainDataValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetPlainDataValueAsFloat(const Property* prop) const -> float;
    [[nodiscard]] auto GetValueAsInt(int property_index) const -> int;
    [[nodiscard]] auto GetValueAsFloat(int property_index) const -> float;
    [[nodiscard]] auto SavePropertyToText(const Property* prop) const -> string;
    [[nodiscard]] auto SaveToText(Properties* base) const -> map<string, string>;

    auto LoadFromText(const map<string, string>& key_values) -> bool;
    auto LoadPropertyFromText(const Property* prop, string_view text) -> bool;
    auto StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint;
    void RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes);
    void RestoreData(const vector<vector<uchar>>& all_data);
    void SetSendIgnore(const Property* prop, const Entity* entity);
    void SetRawData(const Property* prop, const uchar* data, uint data_size);
    void SetValueFromData(const Property* prop, const uchar* data, uint data_size);
    void SetPlainDataValueAsInt(const Property* prop, int value);
    void SetPlainDataValueAsFloat(const Property* prop, float value);
    void SetValueAsInt(int property_index, int value);
    void SetValueAsFloat(int property_index, float value);
    void SetValueAsIntByName(string_view property_name, int value);
    void SetValueAsIntProps(int property_index, int value);

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);
        return *reinterpret_cast<T*>(&_podData[prop->_podDataOffset]);
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);
        T old_value = *reinterpret_cast<T*>(_podData[prop->_podDataOffset]);
        if (new_value != old_value) {
            *reinterpret_cast<T*>(&_podData[prop->_podDataOffset]) = new_value;
            // setCallback(enumValue, old_value)
        }
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, hstring>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(sizeof(hstring::hash_t) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);
        RUNTIME_ASSERT(prop->_isHash);
        const auto h = *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset]);
        // Todo: ResolveHash
        UNUSED_VARIABLE(h);
        return hstring();
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, hstring>, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        RUNTIME_ASSERT(sizeof(hstring::hash_t) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);
        RUNTIME_ASSERT(prop->_isHash);
        const auto old_value = *reinterpret_cast<hstring::hash_t*>(_podData[prop->_podDataOffset]);
        if (new_value.as_uint() != old_value) {
            *reinterpret_cast<hstring::hash_t*>(&_podData[prop->_podDataOffset]) = new_value.as_uint();
            // setCallback(enumValue, old_value)
        }
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        const auto data_size = _complexDataSizes[prop->_complexDataIndex];
        return data_size > 0u ? string(reinterpret_cast<char*>(_complexData[prop->_complexDataIndex]), data_size) : string();
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    void SetValue(const Property* prop, T /*new_value*/)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    void SetValue(const Property* prop, T /*new_value*/)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    [[nodiscard]] auto GetValue(const Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    void SetValue(const Property* prop, T new_value)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    }

private:
    const PropertyRegistrator* _registrator;
    uchar* _podData {};
    vector<uchar*> _complexData {};
    vector<uint> _complexDataSizes {};
    mutable vector<uchar*> _storeData {};
    mutable vector<uint> _storeDataSizes {};
    mutable vector<ushort> _storeDataComplexIndicies {};
    const Entity* _sendIgnoreEntity {};
    const Property* _sendIgnoreProperty {};
    bool _nonConstHelper {};
};

class PropertyRegistrator final
{
    friend class Properties;
    friend class Property;
    friend class PropertiesSerializator;

public:
    PropertyRegistrator() = delete;
    PropertyRegistrator(string_view class_name, bool is_server);
    PropertyRegistrator(const PropertyRegistrator&) = delete;
    PropertyRegistrator(PropertyRegistrator&&) noexcept = default;
    auto operator=(const PropertyRegistrator&) = delete;
    auto operator=(PropertyRegistrator&&) noexcept = delete;
    ~PropertyRegistrator();

    [[nodiscard]] auto GetClassName() const -> const string&;
    [[nodiscard]] auto GetCount() const -> uint;
    [[nodiscard]] auto Find(string_view property_name) const -> const Property*;
    [[nodiscard]] auto GetByIndex(int property_index) const -> const Property*;
    [[nodiscard]] auto IsComponentRegistered(hstring component_name) const -> bool;
    [[nodiscard]] auto GetWholeDataSize() const -> uint;
    [[nodiscard]] auto GetPropertyGroups() const -> const map<string, vector<const Property*>>&;

    void RegisterComponent(hstring name);
    void SetNativeSetCallback(string_view property_name, const NativeCallback& callback);

private:
    string _className;
    bool _isServer;
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
    uint _complexPropertiesCount {};
    vector<ushort> _publicComplexDataProps {};
    vector<ushort> _protectedComplexDataProps {};
    vector<ushort> _publicProtectedComplexDataProps {};
    vector<ushort> _privateComplexDataProps {};

public:
    template<typename T>
    void Register(Property::AccessType access, string_view name, const vector<string>& flags)
    {
        auto* prop = new Property(this);

        prop->_propName = name;
        prop->_accessType = access;
        prop->_dataType = TypeInfo<T>::DATA_TYPE;

        using base_type = typename TypeInfo<T>::BaseType::Type;

        if constexpr (!std::is_same_v<base_type, string>) {
            static_assert(sizeof(base_type) == 1u || sizeof(base_type) == 2u || sizeof(base_type) == 4u || sizeof(base_type) == 8u);
            prop->_baseSize = sizeof(base_type);
            prop->_isHash = TypeInfo<T>::BaseType::IS_HASH;
            prop->_isEnum = TypeInfo<T>::BaseType::IS_ENUM;

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

            if (prop->_isHash) {
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
            prop->_isDictOfString = (!prop->_isDictOfArray && std::is_same_v<base_type, string>);
            prop->_isDictOfArrayOfString = (prop->_isDictOfArray && std::is_same_v<base_type, string>);
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

        AppendProperty(prop, flags);
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

    void AppendProperty(Property* prop, const vector<string>& flags);
};
