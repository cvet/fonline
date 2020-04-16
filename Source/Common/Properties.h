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

#include "Testing.h"

DECLARE_EXCEPTION(PropertiesException);

class Entity;
class Property;
class PropertyRegistrator;
class Properties;
using PropertyVec = vector<Property*>;
using NativeSendCallback = std::function<void(Entity*, Property*)>;
using NativeCallback = std::function<void(Entity*, Property*, void*, void*)>;
using NativeCallbackVec = vector<NativeCallback>;

class Property : public NonCopyable
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

    string GetName();
    string GetTypeName();
    uint GetRegIndex();
    int GetEnumValue();
    AccessType GetAccess();
    uint GetBaseSize();
    bool IsPOD();
    bool IsDict();
    bool IsHash();
    bool IsResource();
    bool IsEnum();
    bool IsReadable();
    bool IsWritable();
    bool IsConst();
    bool IsTemporary();
    bool IsNoHistory();

private:
    enum DataType
    {
        POD,
        String,
        Array,
        Dict,
    };

    Property() = default;
    unique_ptr<void, std::function<void(void*)>> CreateRefValue(uchar* data, uint data_size);
    uchar* ExpandComplexValueData(void* pvalue, uint& data_size, bool& need_delete);

    size_t typeHash {};
    string propName {};
    string typeName {};
    string componentName {};
    DataType dataType {};
    bool isHash {};
    bool isHashSubType0 {};
    bool isHashSubType1 {};
    bool isHashSubType2 {};
    bool isResource {};
    bool isIntDataType {};
    bool isSignedIntDataType {};
    bool isFloatDataType {};
    bool isBoolDataType {};
    bool isEnumDataType {};
    bool isArrayOfString {};
    bool isDictOfString {};
    bool isDictOfArray {};
    bool isDictOfArrayOfString {};
    AccessType accessType {};
    bool isConst {};
    bool isReadable {};
    bool isWritable {};
    bool checkMinValue {};
    bool checkMaxValue {};
    int64 minValue {};
    int64 maxValue {};
    bool isTemporary {};
    bool isNoHistory {};
    PropertyRegistrator* registrator {};
    uint regIndex {};
    uint getIndex {};
    int enumValue {};
    uint podDataOffset {};
    uint complexDataIndex {};
    uint baseSize {};
};

class Properties
{
    friend class PropertyRegistrator;
    friend class Property;
    friend class PropertiesSerializator;

public:
    Properties(PropertyRegistrator* reg);
    Properties(const Properties& other);
    ~Properties();
    Properties& operator=(const Properties& other);
    Property* FindByEnum(int enum_value);
    void* FindData(const string& property_name);
    uint StoreData(bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes);
    void RestoreData(PUCharVec& all_data, UIntVec& all_data_sizes);
    void RestoreData(UCharVecVec& all_data);
    bool LoadFromText(const StrMap& key_values);
    void SaveToText(StrMap& key_values, Properties* base);
    bool LoadPropertyFromText(Property* prop, const string& text);
    string SavePropertyToText(Property* prop);

    void SetSendIgnore(Property* prop, Entity* entity);
    PropertyRegistrator* GetRegistrator() { return registrator; }

    uint GetRawDataSize(Property* prop);
    uchar* GetRawData(Property* prop, uint& data_size);
    void SetRawData(Property* prop, uchar* data, uint data_size);
    void SetValueFromData(Property* prop, uchar* data, uint data_size);
    int GetPODValueAsInt(Property* prop);
    void SetPODValueAsInt(Property* prop, int value);

    int GetValueAsInt(int enum_value);
    void SetValueAsInt(int enum_value, int value);
    void SetValueAsIntByName(const string& enum_name, int value);
    void SetValueAsIntProps(int enum_value, int value);

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    T GetValue(Property* prop)
    {
        RUNTIME_ASSERT(sizeof(T) == prop->baseSize);
        RUNTIME_ASSERT(prop->dataType == Property::DataType::POD);
        return *reinterpret_cast<T*>(podData[prop->podDataOffset]);
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(sizeof(T) == prop->baseSize);
        RUNTIME_ASSERT(prop->dataType == Property::DataType::POD);
        T old_value = *reinterpret_cast<T*>(podData[prop->podDataOffset]);
        if (new_value != old_value)
        {
            *reinterpret_cast<T*>(podData[prop->podDataOffset]) = new_value;
            // setCallback(enumValue, old_value)
        }
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    T GetValue(Property* prop)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
        uint data_size = complexDataSizes[prop->complexDataIndex];
        return data_size ? string(reinterpret_cast<char*>(podData[prop->complexDataIndex]), data_size) : string();
    }

    template<typename T, std::enable_if_t<std::is_same_v<T, string>, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::String);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
    }

    template<typename Test, template<typename...> class Ref>
    struct is_specialization : std::false_type
    {
    };

    template<template<typename...> class Ref, typename... Args>
    struct is_specialization<Ref<Args...>, Ref> : std::true_type
    {
    };

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    T GetValue(Property* prop)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, vector>::value, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    T GetValue(Property* prop)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
        return {};
    }

    template<typename T, std::enable_if_t<is_specialization<T, map>::value, int> = 0>
    void SetValue(Property* prop, T new_value)
    {
        RUNTIME_ASSERT(prop->dataType == Property::DataType::Array);
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
    }

private:
    Properties();

    PropertyRegistrator* registrator {};
    uchar* podData {};
    PUCharVec complexData {};
    UIntVec complexDataSizes {};
    PUCharVec storeData {};
    UIntVec storeDataSizes {};
    UShortVec storeDataComplexIndicies {};
    Entity* sendIgnoreEntity {};
    Property* sendIgnoreProperty {};
};

class PropertyRegistrator
{
    friend class Properties;
    friend class Property;
    friend class PropertiesSerializator;

public:
    PropertyRegistrator(bool is_server);
    ~PropertyRegistrator();
    Property* Register(Property::AccessType access, const type_info& type, const string& name);
    void RegisterComponent(const string& name);
    string GetClassName();
    uint GetCount();
    Property* Find(const string& property_name);
    Property* FindByEnum(int enum_value);
    Property* Get(uint property_index);
    bool IsComponentRegistered(hash component_name);
    void SetNativeSetCallback(const string& property_name, NativeCallback callback);
    uint GetWholeDataSize();

private:
    bool isServer {};
    string className {};
    vector<Property*> registeredProperties {};
    HashSet registeredComponents {};
    uint getPropertiesCount {};

    // POD info
    uint wholePodDataSize {};
    BoolVec publicPodDataSpace {};
    BoolVec protectedPodDataSpace {};
    BoolVec privatePodDataSpace {};
    PUCharVec podDataPool {};

    // Complex types info
    uint complexPropertiesCount {};
    UShortVec publicComplexDataProps {};
    UShortVec protectedComplexDataProps {};
    UShortVec publicProtectedComplexDataProps {};
    UShortVec privateComplexDataProps {};
};
