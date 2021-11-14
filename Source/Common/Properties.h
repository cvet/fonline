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

DECLARE_EXCEPTION(PropertiesException);

class Entity;
class Property;
class PropertyRegistrator;
class Properties;
using PropertyVec = vector<Property*>;
using NativeSendCallback = std::function<void(Entity*, Property*)>;
using NativeCallback = std::function<void(Entity*, Property*, void*, void*)>;
using NativeCallbackVec = vector<NativeCallback>;

class Property final
{
    friend class PropertyRegistrator;
    friend class Properties;
    friend class PropertiesSerializator;

public:
    enum AccessType
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

    [[nodiscard]] auto GetName() const -> string;
    [[nodiscard]] auto GetTypeName() const -> string;
    [[nodiscard]] auto GetRegIndex() const -> uint;
    [[nodiscard]] auto GetEnumValue() const -> int;
    [[nodiscard]] auto GetAccess() const -> AccessType;
    [[nodiscard]] auto GetBaseSize() const -> uint;
    [[nodiscard]] auto IsPOD() const -> bool;
    [[nodiscard]] auto IsDict() const -> bool;
    [[nodiscard]] auto IsHash() const -> bool;
    [[nodiscard]] auto IsResource() const -> bool;
    [[nodiscard]] auto IsEnum() const -> bool;
    [[nodiscard]] auto IsReadable() const -> bool;
    [[nodiscard]] auto IsWritable() const -> bool;
    [[nodiscard]] auto IsConst() const -> bool;
    [[nodiscard]] auto IsTemporary() const -> bool;
    [[nodiscard]] auto IsNoHistory() const -> bool;

private:
    enum DataType
    {
        POD,
        String,
        Array,
        Dict,
    };

    Property() = default;
    static auto CreateRefValue(uchar* data, uint data_size) -> unique_ptr<void, std::function<void(void*)>>;
    static auto ExpandComplexValueData(void* pvalue, uint& data_size, bool& need_delete) -> uchar*;

    size_t _typeHash {};
    string _propName {};
    string _typeName {};
    string _componentName {};
    DataType _dataType {};
    bool _isHash {};
    bool _isHashSubType0 {};
    bool _isHashSubType1 {};
    bool _isHashSubType2 {};
    bool _isResource {};
    bool _isIntDataType {};
    bool _isSignedIntDataType {};
    bool _isFloatDataType {};
    bool _isBoolDataType {};
    bool _isEnumDataType {};
    bool _isArrayOfString {};
    bool _isDictOfString {};
    bool _isDictOfArray {};
    bool _isDictOfArrayOfString {};
    AccessType _accessType {};
    bool _isConst {};
    bool _isReadable {};
    bool _isWritable {};
    bool _checkMinValue {};
    bool _checkMaxValue {};
    int64 _minValue {};
    int64 _maxValue {};
    bool _isTemporary {};
    bool _isNoHistory {};
    PropertyRegistrator* _registrator {};
    uint _regIndex {};
    uint _getIndex {};
    int _enumValue {};
    uint _podDataOffset {};
    uint _complexDataIndex {};
    uint _baseSize {};
};

class Properties final
{
    friend class PropertyRegistrator;
    friend class Property;
    friend class PropertiesSerializator;

public:
    Properties() = delete;
    explicit Properties(PropertyRegistrator* reg);
    Properties(const Properties& other);
    Properties(Properties&&) noexcept = default;
    auto operator=(const Properties& other) -> Properties&;
    auto operator=(Properties&&) noexcept = delete;
    ~Properties();

    [[nodiscard]] auto GetRegistrator() const -> PropertyRegistrator* { return _registrator; }
    [[nodiscard]] auto GetRawDataSize(Property* prop) const -> uint;
    [[nodiscard]] auto GetRawData(Property* prop, uint& data_size) const -> const uchar*;
    [[nodiscard]] auto GetRawData(Property* prop, uint& data_size) -> uchar*;
    [[nodiscard]] auto GetPODValueAsInt(Property* prop) const -> int;
    [[nodiscard]] auto GetValueAsInt(int enum_value) const -> int;
    [[nodiscard]] auto FindByEnum(int enum_value) -> Property*;
    [[nodiscard]] auto FindData(string_view property_name) -> void*;

    [[nodiscard]] auto SavePropertyToText(Property* prop) const -> string;
    [[nodiscard]] auto SaveToText(Properties* base) const -> map<string, string>;

    auto LoadFromText(const map<string, string>& key_values) -> bool;
    auto LoadPropertyFromText(Property* prop, string_view text) -> bool;
    auto StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint;
    void RestoreData(vector<uchar*>& all_data, vector<uint>& all_data_sizes);
    void RestoreData(vector<vector<uchar>>& all_data);
    void SetSendIgnore(Property* prop, Entity* entity);
    void SetRawData(Property* prop, uchar* data, uint data_size);
    void SetValueFromData(Property* prop, uchar* data, uint data_size);
    void SetPODValueAsInt(Property* prop, int value);
    void SetValueAsInt(int enum_value, int value);
    void SetValueAsIntByName(string_view enum_name, int value);
    void SetValueAsIntProps(int enum_value, int value);

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    [[nodiscard]] auto GetValue(Property* prop) const -> T
    {
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::POD);
        return *reinterpret_cast<T*>(_podData[prop->_podDataOffset]);
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(sizeof(T) == prop->_baseSize);
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::POD);
        T old_value = *reinterpret_cast<T*>(_podData[prop->_podDataOffset]);
        if (new_value != old_value) {
            *reinterpret_cast<T*>(_podData[prop->_podDataOffset]) = new_value;
            // setCallback(enumValue, old_value)
        }
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    [[nodiscard]] auto GetValue(Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
        const auto data_size = _complexDataSizes[prop->_complexDataIndex];
        return data_size ? string(reinterpret_cast<char*>(_complexData[prop->_complexDataIndex]), data_size) : string();
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    void SetValue(Property* prop, T /*new_value*/)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    [[nodiscard]] auto GetValue(Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    void SetValue(Property* prop, T /*new_value*/)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    [[nodiscard]] auto GetValue(Property* prop) const -> T
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(prop->_dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->_complexDataIndex != uint(-1));
    }

private:
    PropertyRegistrator* _registrator {};
    uchar* _podData {};
    vector<uchar*> _complexData {};
    vector<uint> _complexDataSizes {};
    mutable vector<uchar*> _storeData {};
    mutable vector<uint> _storeDataSizes {};
    mutable vector<ushort> _storeDataComplexIndicies {};
    Entity* _sendIgnoreEntity {};
    Property* _sendIgnoreProperty {};
    bool _nonConstHelper {};
};

class PropertyRegistrator final
{
    friend class Properties;
    friend class Property;
    friend class PropertiesSerializator;

public:
    PropertyRegistrator() = delete;
    explicit PropertyRegistrator(bool is_server);
    PropertyRegistrator(const PropertyRegistrator&) = delete;
    PropertyRegistrator(PropertyRegistrator&&) noexcept = default;
    auto operator=(const PropertyRegistrator&) = delete;
    auto operator=(PropertyRegistrator&&) noexcept = delete;
    ~PropertyRegistrator();

    [[nodiscard]] auto GetClassName() const -> string;
    [[nodiscard]] auto GetCount() const -> uint;
    [[nodiscard]] auto Find(string_view property_name) -> Property*;
    [[nodiscard]] auto FindByEnum(int enum_value) -> Property*;
    [[nodiscard]] auto Get(uint property_index) -> Property*;
    [[nodiscard]] auto IsComponentRegistered(hash component_name) const -> bool;
    [[nodiscard]] auto GetWholeDataSize() const -> uint;

    auto Register(Property::AccessType access, const type_info& type, string_view name) -> Property*;
    void RegisterComponent(string_view name);
    void SetNativeSetCallback(string_view property_name, const NativeCallback& callback);

private:
    bool _isServer {};
    string _className {};
    vector<Property*> _registeredProperties {};
    set<hash> _registeredComponents {};
    uint _getPropertiesCount {};

    // POD info
    uint _wholePodDataSize {};
    vector<bool> _publicPodDataSpace {};
    vector<bool> _protectedPodDataSpace {};
    vector<bool> _privatePodDataSpace {};
    vector<uchar*> _podDataPool {};

    // Complex types info
    uint _complexPropertiesCount {};
    vector<ushort> _publicComplexDataProps {};
    vector<ushort> _protectedComplexDataProps {};
    vector<ushort> _publicProtectedComplexDataProps {};
    vector<ushort> _privateComplexDataProps {};
};
