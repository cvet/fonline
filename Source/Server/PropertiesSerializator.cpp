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

#include "PropertiesSerializator.h"
#include "Log.h"
#include "StringUtils.h"

auto PropertiesSerializator::SaveToDbDocument(const Properties* props, const Properties* base, const NameResolver& name_resolver) -> DataBase::Document
{
    RUNTIME_ASSERT(!base || props->_registrator == base->_registrator);

    DataBase::Document doc;
    for (auto& prop : props->_registrator->_registeredProperties) {
        // Skip pure virtual properties
        if (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1)) {
            continue;
        }

        // Skip temporary properties
        if (prop->_isTemporary) {
            continue;
        }

        // Skip same
        if (base != nullptr) {
            if (prop->_podDataOffset != static_cast<uint>(-1)) {
                if (memcmp(&props->_podData[prop->_podDataOffset], &base->_podData[prop->_podDataOffset], prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (props->_complexDataSizes[prop->_complexDataIndex] == 0u && base->_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
                if (props->_complexDataSizes[prop->_complexDataIndex] == base->_complexDataSizes[prop->_complexDataIndex] && memcmp(props->_complexData[prop->_complexDataIndex], base->_complexData[prop->_complexDataIndex], props->_complexDataSizes[prop->_complexDataIndex]) == 0) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset != static_cast<uint>(-1)) {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(prop->_baseSize <= sizeof(pod_zero));
                if (memcmp(&props->_podData[prop->_podDataOffset], &pod_zero, prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (props->_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
            }
        }

        doc.insert(std::make_pair(prop->_propName, SavePropertyToDbValue(props, prop, name_resolver)));
    }

    return doc;
}

auto PropertiesSerializator::LoadFromDbDocument(Properties* /*props*/, const DataBase::Document& /*doc*/, const NameResolver & /*name_resolver*/) -> bool
{
    /*bool is_error = false;
    for (const auto& kv : doc)
    {
        string_view key = kv.first;
        const DataBase::Value& value = kv.second;

        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_')
            continue;

        // Find property
        Property* prop = props->registrator->Find(key);
        if (!prop || (prop->podDataOffset == uint(-1) && prop->complexDataIndex == uint(-1)))
        {
            if (!prop)
                WriteLog("Unknown property '{}'.\n", key);
            else
                WriteLog("Invalid property '{}' for reading.\n", prop->GetName());

            is_error = true;
            continue;
        }

        // Parse value
        if (prop->isEnumDataType)
        {
            if (value.index() != DataBase::StringValue)
            {
                WriteLog("Wrong enum value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            int e = name_resolver.GetEnumValue(prop->asObjType->GetName(), std::get<string>(value), is_error);
            prop->SetPropRawData(props, (uchar*)&e, prop->baseSize);
        }
        else if (prop->isHash || prop->isResource)
        {
            if (value.index() != DataBase::StringValue)
            {
                WriteLog("Wrong hash value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            hash h = _str(std::get<string>(value)).toHash();
            prop->SetPropRawData(props, (uchar*)&h, prop->baseSize);
        }
        else if (prop->isIntDataType || prop->isFloatDataType || prop->isBoolDataType)
        {
            if (value.index() == DataBase::StringValue || value.index() == DataBase::ArrayValue ||
                value.index() == DataBase::DictValue)
            {
                WriteLog("Wrong integer value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

#define PARSE_VALUE(t) \
    do \
    { \
        if (prop->asObjTypeId == asTYPEID_INT8) \
            *(char*)pod_data = (char)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_INT16) \
            *(short*)pod_data = (short)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_INT32) \
            *(int*)pod_data = (int)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_INT64) \
            *(int64*)pod_data = (int64)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_UINT8) \
            *(uchar*)pod_data = (uchar)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_UINT16) \
            *(short*)pod_data = (short)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_UINT32) \
            *(uint*)pod_data = (uint)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_UINT64) \
            *(uint64*)pod_data = (uint64)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_FLOAT) \
            *(float*)pod_data = (float)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_DOUBLE) \
            *(double*)pod_data = (double)std::get<t>(value); \
        else if (prop->asObjTypeId == asTYPEID_BOOL) \
            *(bool*)pod_data = std::get<t>(value) != 0; \
        else \
            throw UnreachablePlaceException(LINE_STR); \
    } while (false)

            uchar pod_data[8];
            if (value.index() == DataBase::IntValue)
                PARSE_VALUE(int);
            else if (value.index() == DataBase::Int64Value)
                PARSE_VALUE(int64);
            else if (value.index() == DataBase::DoubleValue)
                PARSE_VALUE(double);
            else if (value.index() == DataBase::BoolValue)
                PARSE_VALUE(bool);
            else
                throw UnreachablePlaceException(LINE_STR);

#undef PARSE_VALUE

            prop->SetPropRawData(props, pod_data, prop->baseSize);
        }
        else if (prop->dataType == Property::String)
        {
            if (value.index() != DataBase::StringValue)
            {
                WriteLog("Wrong string value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            prop->SetPropRawData(
                props, (uchar*)std::get<string>(value).c_str(), (uint)std::get<string>(value).length());
        }
        else if (prop->dataType == Property::Array)
        {
            if (value.index() != DataBase::ArrayValue)
            {
                WriteLog("Wrong array value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            const DataBase::Array& arr = std::get<DataBase::Array>(value);
            if (arr.empty())
            {
                prop->SetPropRawData(props, nullptr, 0);
                continue;
            }

            if (prop->isHashSubType0)
            {
                if (arr[0].index() != DataBase::StringValue)
                {
                    WriteLog("Wrong array hash element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = (uint)arr.size() * sizeof(hash);
                uchar* data = new uchar[data_size];
                for (size_t i = 0; i < arr.size(); i++)
                {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::StringValue);

                    hash h = _str(std::get<string>(arr[i])).toHash();
                    *(hash*)(data + i * sizeof(hash)) = h;
                }

                prop->SetPropRawData(props, data, data_size);
                delete[] data;
            }
            else if (!(prop->asObjType->GetSubTypeId() & asTYPEID_MASK_OBJECT) &&
                prop->asObjType->GetSubTypeId() > asTYPEID_DOUBLE)
            {
                if (arr[0].index() != DataBase::StringValue)
                {
                    WriteLog("Wrong array enum element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                string enum_name = prop->asObjType->GetSubType()->GetName();
                uint data_size = (uint)arr.size() * sizeof(int);
                uchar* data = new uchar[data_size];
                for (size_t i = 0; i < arr.size(); i++)
                {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::StringValue);

                    int e = name_resolver.GetEnumValue(enum_name, std::get<string>(arr[i]), is_error);
                    *(int*)(data + i * sizeof(int)) = e;
                }

                prop->SetPropRawData(props, data, data_size);
                delete[] data;
            }
            else if (!(prop->asObjType->GetSubTypeId() & asTYPEID_MASK_OBJECT))
            {
                if (arr[0].index() == DataBase::StringValue || arr[0].index() == DataBase::ArrayValue ||
                    arr[0].index() == DataBase::DictValue)
                {
                    WriteLog("Wrong array element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                int element_type_id = prop->asObjType->GetSubTypeId();
                int element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(element_type_id);
                uint data_size = element_size * (int)arr.size();
                uchar* data = new uchar[data_size];
                size_t arr_element_index = arr[0].index();

#define PARSE_VALUE(t) \
    do \
    { \
        for (size_t i = 0; i < arr.size(); i++) \
        { \
            RUNTIME_ASSERT(arr[i].index() == arr_element_index); \
            if (arr_element_index == DataBase::IntValue) \
                *(t*)(data + i * element_size) = (t)std::get<int>(arr[i]); \
            else if (arr_element_index == DataBase::Int64Value) \
                *(t*)(data + i * element_size) = (t)std::get<int64>(arr[i]); \
            else if (arr_element_index == DataBase::DoubleValue) \
                *(t*)(data + i * element_size) = (t)std::get<double>(arr[i]); \
            else if (arr_element_index == DataBase::BoolValue) \
                *(t*)(data + i * element_size) = (t)std::get<bool>(arr[i]); \
            else \
                throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                if (element_type_id == asTYPEID_INT8)
                    PARSE_VALUE(char);
                else if (element_type_id == asTYPEID_INT16)
                    PARSE_VALUE(short);
                else if (element_type_id == asTYPEID_INT32)
                    PARSE_VALUE(int);
                else if (element_type_id == asTYPEID_INT64)
                    PARSE_VALUE(int64);
                else if (element_type_id == asTYPEID_UINT8)
                    PARSE_VALUE(uchar);
                else if (element_type_id == asTYPEID_UINT16)
                    PARSE_VALUE(ushort);
                else if (element_type_id == asTYPEID_UINT32)
                    PARSE_VALUE(uint);
                else if (element_type_id == asTYPEID_UINT64)
                    PARSE_VALUE(uint64);
                else if (element_type_id == asTYPEID_FLOAT)
                    PARSE_VALUE(float);
                else if (element_type_id == asTYPEID_DOUBLE)
                    PARSE_VALUE(double);
                else if (element_type_id == asTYPEID_BOOL)
                    PARSE_VALUE(bool);
                else
                    throw UnreachablePlaceException(LINE_STR);

#undef PARSE_VALUE

                prop->SetPropRawData(props, data, data_size);
                delete[] data;
            }
            else
            {
                RUNTIME_ASSERT(prop->isArrayOfString);

                if (arr[0].index() != DataBase::StringValue)
                {
                    WriteLog("Wrong array element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = sizeof(uint);
                for (size_t i = 0; i < arr.size(); i++)
                {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::StringValue);

                    string_view str = std::get<string>(arr[i]);
                    data_size += sizeof(uint) + (uint)str.length();
                }

                uchar* data = new uchar[data_size];
                *(uint*)data = (uint)arr.size();
                size_t data_pos = sizeof(uint);
                for (size_t i = 0; i < arr.size(); i++)
                {
                    string_view str = std::get<string>(arr[i]);
                    *(uint*)(data + data_pos) = (uint)str.length();
                    if (!str.empty())
                        std::memcpy(data + data_pos + sizeof(uint), str.c_str(), str.length());
                    data_pos += sizeof(uint) + str.length();
                }
                RUNTIME_ASSERT(data_pos == data_size);

                prop->SetPropRawData(props, data, data_size);
                delete[] data;
            }
        }
        else if (prop->dataType == Property::Dict)
        {
            if (value.index() != DataBase::DictValue)
            {
                WriteLog("Wrong dict value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            const DataBase::Dict& dict = std::get<DataBase::Dict>(value);
            if (dict.empty())
            {
                prop->SetPropRawData(props, nullptr, 0);
                continue;
            }

            int key_element_type_id = prop->asObjType->GetSubTypeId();
            RUNTIME_ASSERT(!(key_element_type_id & asTYPEID_MASK_OBJECT));
            int key_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(key_element_type_id);
            string key_element_type_name =
                prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            int value_element_type_id = prop->asObjType->GetSubTypeId(1);
            int value_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(value_element_type_id);
            string value_element_type_name =
                prop->asObjType->GetSubType(1) ? prop->asObjType->GetSubType(1)->GetName() : "";

            // Measure data length
            uint data_size = 0;
            bool wrong_input = false;
            for (auto& kv : dict)
            {
                data_size += key_element_size;

                if (prop->isDictOfArray)
                {
                    if (kv.second.index() != DataBase::ArrayValue)
                    {
                        WriteLog("Wrong dict array value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    const DataBase::Array& arr = std::get<DataBase::Array>(kv.second);
                    int arr_element_type_id = prop->asObjType->GetSubType(1)->GetSubTypeId();
                    int arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(arr_element_type_id);

                    data_size += sizeof(uint);

                    if (!(arr_element_type_id & asTYPEID_MASK_OBJECT) && arr_element_type_id > asTYPEID_DOUBLE)
                    {
                        for (auto& e : arr)
                        {
                            if (e.index() != DataBase::StringValue)
                            {
                                WriteLog("Wrong dict array element enum value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (uint)arr.size() * sizeof(int);
                    }
                    else if (prop->isHashSubType2)
                    {
                        for (auto& e : arr)
                        {
                            if (e.index() != DataBase::StringValue)
                            {
                                WriteLog("Wrong dict array element hash value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (uint)arr.size() * sizeof(hash);
                    }
                    else if (prop->isDictOfArrayOfString)
                    {
                        for (auto& e : arr)
                        {
                            if (e.index() != DataBase::StringValue)
                            {
                                WriteLog(
                                    "Wrong dict array element string value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }

                            data_size += sizeof(uint) + (uint)std::get<string>(e).length();
                        }
                    }
                    else
                    {
                        for (auto& e : arr)
                        {
                            if ((e.index() != DataBase::IntValue && e.index() != DataBase::Int64Value &&
                                    e.index() != DataBase::DoubleValue && e.index() != DataBase::BoolValue) ||
                                e.index() != arr[0].index())
                            {
                                WriteLog("Wrong dict array element value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += (int)arr.size() * arr_element_size;
                    }
                }
                else if (prop->isDictOfString)
                {
                    if (kv.second.index() != DataBase::StringValue)
                    {
                        WriteLog("Wrong dict string element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += (uint)std::get<string>(kv.second).length();
                }
                else if (!(value_element_type_id & asTYPEID_MASK_OBJECT) && value_element_type_id > asTYPEID_DOUBLE)
                {
                    if (kv.second.index() != DataBase::StringValue)
                    {
                        WriteLog("Wrong dict enum element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    RUNTIME_ASSERT(value_element_size == sizeof(int));
                    data_size += sizeof(int);
                }
                else if (prop->isHashSubType1)
                {
                    if (kv.second.index() != DataBase::StringValue)
                    {
                        WriteLog("Wrong dict hash element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    RUNTIME_ASSERT(value_element_size == sizeof(hash));
                    data_size += sizeof(hash);
                }
                else
                {
                    if (kv.second.index() != DataBase::IntValue && kv.second.index() != DataBase::Int64Value &&
                        kv.second.index() != DataBase::DoubleValue && kv.second.index() != DataBase::BoolValue)
                    {
                        WriteLog("Wrong dict number element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += value_element_size;
                }

                if (wrong_input)
                    break;
            }

            if (wrong_input)
            {
                is_error = true;
                continue;
            }

            // Write data
            uchar* data = new uchar[data_size];
            size_t data_pos = 0;
            for (auto& kv : dict)
            {
                if (prop->isHashSubType0)
                    *(hash*)(data + data_pos) = _str(kv.first).toHash();
                else if (key_element_type_id == asTYPEID_INT8)
                    *(char*)(data + data_pos) = (char)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_INT16)
                    *(short*)(data + data_pos) = (short)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_INT32)
                    *(int*)(data + data_pos) = (int)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_INT64)
                    *(int64*)(data + data_pos) = (int64)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_UINT8)
                    *(uchar*)(data + data_pos) = (uchar)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_UINT16)
                    *(ushort*)(data + data_pos) = (ushort)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_UINT32)
                    *(uint*)(data + data_pos) = (uint)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_UINT64)
                    *(uint64*)(data + data_pos) = (uint64)_str(kv.first).toInt64();
                else if (key_element_type_id == asTYPEID_FLOAT)
                    *(float*)(data + data_pos) = (float)_str(kv.first).toDouble();
                else if (key_element_type_id == asTYPEID_DOUBLE)
                    *(double*)(data + data_pos) = (double)_str(kv.first).toDouble();
                else if (key_element_type_id == asTYPEID_BOOL)
                    *(bool*)(data + data_pos) = (bool)_str(kv.first).toBool();
                else if (!(key_element_type_id & asTYPEID_MASK_OBJECT) && key_element_type_id > asTYPEID_DOUBLE)
                    *(int*)(data + data_pos) = name_resolver.GetEnumValue(key_element_type_name, kv.first, is_error);
                else
                    throw UnreachablePlaceException(LINE_STR);

                data_pos += key_element_size;

                if (prop->isDictOfArray)
                {
                    const DataBase::Array& arr = std::get<DataBase::Array>(kv.second);

                    *(uint*)(data + data_pos) = (uint)arr.size();
                    data_pos += sizeof(uint);

                    int arr_element_type_id = prop->asObjType->GetSubType(1)->GetSubTypeId();
                    int arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(arr_element_type_id);

                    if (!(arr_element_type_id & asTYPEID_MASK_OBJECT) && arr_element_type_id > asTYPEID_DOUBLE)
                    {
                        string arr_element_type_name = prop->asObjType->GetSubType(1)->GetSubType()->GetName();
                        for (auto& e : arr)
                        {
                            *(int*)(data + data_pos) =
                                name_resolver.GetEnumValue(arr_element_type_name, std::get<string>(e), is_error);
                            data_pos += sizeof(int);
                        }
                    }
                    else if (prop->isHashSubType2)
                    {
                        for (auto& e : arr)
                        {
                            *(hash*)(data + data_pos) = _str(std::get<string>(e)).toHash();
                            data_pos += sizeof(hash);
                        }
                    }
                    else if (prop->isDictOfArrayOfString)
                    {
                        for (auto& e : arr)
                        {
                            string_view str = std::get<string>(e);
                            *(uint*)(data + data_pos) = (uint)str.length();
                            data_pos += sizeof(uint);
                            if (!str.empty())
                            {
                                std::memcpy(data + data_pos, str.c_str(), str.length());
                                data_pos += (uint)str.length();
                            }
                        }
                    }
                    else
                    {
                        for (auto& e : arr)
                        {
#define PARSE_VALUE(t) \
    do \
    { \
        RUNTIME_ASSERT(sizeof(t) == arr_element_size); \
        if (e.index() == DataBase::IntValue) \
            *(t*)(data + data_pos) = (t)std::get<int>(e); \
        else if (e.index() == DataBase::Int64Value) \
            *(t*)(data + data_pos) = (t)std::get<int64>(e); \
        else if (e.index() == DataBase::DoubleValue) \
            *(t*)(data + data_pos) = (t)std::get<double>(e); \
        else if (e.index() == DataBase::BoolValue) \
            *(t*)(data + data_pos) = (t)std::get<bool>(e); \
        else \
            throw UnreachablePlaceException(LINE_STR); \
    } while (false)

                            if (arr_element_type_id == asTYPEID_INT8)
                                PARSE_VALUE(char);
                            else if (arr_element_type_id == asTYPEID_INT16)
                                PARSE_VALUE(short);
                            else if (arr_element_type_id == asTYPEID_INT32)
                                PARSE_VALUE(int);
                            else if (arr_element_type_id == asTYPEID_INT64)
                                PARSE_VALUE(int64);
                            else if (arr_element_type_id == asTYPEID_UINT8)
                                PARSE_VALUE(uchar);
                            else if (arr_element_type_id == asTYPEID_UINT16)
                                PARSE_VALUE(ushort);
                            else if (arr_element_type_id == asTYPEID_UINT32)
                                PARSE_VALUE(uint);
                            else if (arr_element_type_id == asTYPEID_UINT64)
                                PARSE_VALUE(uint64);
                            else if (arr_element_type_id == asTYPEID_FLOAT)
                                PARSE_VALUE(float);
                            else if (arr_element_type_id == asTYPEID_DOUBLE)
                                PARSE_VALUE(double);
                            else if (arr_element_type_id == asTYPEID_BOOL)
                                PARSE_VALUE(bool);
                            else
                                throw UnreachablePlaceException(LINE_STR);

#undef PARSE_VALUE

                            data_pos += arr_element_size;
                        }
                    }
                }
                else if (prop->isDictOfString)
                {
                    string_view str = std::get<string>(kv.second);

                    *(uint*)(data + data_pos) = (uint)str.length();
                    data_pos += sizeof(uint);

                    if (!str.empty())
                    {
                        std::memcpy(data + data_pos, str.c_str(), str.length());
                        data_pos += str.length();
                    }
                }
                else
                {
                    RUNTIME_ASSERT(!(value_element_type_id & asTYPEID_MASK_OBJECT));

#define PARSE_VALUE(t) \
    do \
    { \
        RUNTIME_ASSERT(sizeof(t) == value_element_size); \
        if (kv.second.index() == DataBase::IntValue) \
            *(t*)(data + data_pos) = (t)std::get<int>(kv.second); \
        else if (kv.second.index() == DataBase::Int64Value) \
            *(t*)(data + data_pos) = (t)std::get<int64>(kv.second); \
        else if (kv.second.index() == DataBase::DoubleValue) \
            *(t*)(data + data_pos) = (t)std::get<double>(kv.second); \
        else if (kv.second.index() == DataBase::BoolValue) \
            *(t*)(data + data_pos) = (t)std::get<bool>(kv.second); \
        else \
            throw UnreachablePlaceException(LINE_STR); \
    } while (false)

                    if (prop->isHashSubType1)
                        *(hash*)(data + data_pos) = _str(std::get<string>(kv.second)).toHash();
                    else if (value_element_type_id == asTYPEID_INT8)
                        PARSE_VALUE(char);
                    else if (value_element_type_id == asTYPEID_INT16)
                        PARSE_VALUE(short);
                    else if (value_element_type_id == asTYPEID_INT32)
                        PARSE_VALUE(int);
                    else if (value_element_type_id == asTYPEID_INT64)
                        PARSE_VALUE(int64);
                    else if (value_element_type_id == asTYPEID_UINT8)
                        PARSE_VALUE(uchar);
                    else if (value_element_type_id == asTYPEID_UINT16)
                        PARSE_VALUE(ushort);
                    else if (value_element_type_id == asTYPEID_UINT32)
                        PARSE_VALUE(uint);
                    else if (value_element_type_id == asTYPEID_UINT64)
                        PARSE_VALUE(uint64);
                    else if (value_element_type_id == asTYPEID_FLOAT)
                        PARSE_VALUE(float);
                    else if (value_element_type_id == asTYPEID_DOUBLE)
                        PARSE_VALUE(double);
                    else if (value_element_type_id == asTYPEID_BOOL)
                        PARSE_VALUE(bool);
                    else
                        *(int*)(data + data_pos) =
                            name_resolver.GetEnumValue(value_element_type_name, std::get<string>(kv.second), is_error);

#undef PARSE_VALUE

                    data_pos += value_element_size;
                }
            }

            if (data_pos != data_size)
                WriteLog("{} {}\n", data_pos, data_size);
            RUNTIME_ASSERT(data_pos == data_size);

            prop->SetPropRawData(props, data, data_size);
            delete[] data;
        }
        else
        {
            throw UnreachablePlaceException(LINE_STR);
        }
    }
    return !is_error;*/
    return false;
}

auto PropertiesSerializator::SavePropertyToDbValue(const Properties* /*props*/, const Property* /*prop*/, const NameResolver & /*name_resolver*/) -> DataBase::Value
{
    /*RUNTIME_ASSERT(prop->podDataOffset != uint(-1) || prop->complexDataIndex != uint(-1));
    RUNTIME_ASSERT(!prop->isTemporary);

    uint data_size;
    uchar* data = prop->GetPropRawData(props, data_size);

    if (prop->dataType == Property::POD)
    {
        RUNTIME_ASSERT(prop->podDataOffset != uint(-1));

        if (prop->isHash || prop->isResource)
            return _str().parseHash(*(hash*)&props->podData[prop->podDataOffset]).str();

#define PARSE_VALUE(as_t, t, ret_t) \
    if (prop->asObjTypeId == as_t) \
        return (ret_t) * (t*)&props->podData[prop->podDataOffset];

        PARSE_VALUE(asTYPEID_INT8, char, int);
        PARSE_VALUE(asTYPEID_INT16, short, int);
        PARSE_VALUE(asTYPEID_INT32, int, int);
        PARSE_VALUE(asTYPEID_INT64, int64, int64);
        PARSE_VALUE(asTYPEID_UINT8, uchar, int);
        PARSE_VALUE(asTYPEID_UINT16, ushort, int);
        PARSE_VALUE(asTYPEID_UINT32, uint, int);
        PARSE_VALUE(asTYPEID_UINT64, uint64, int64);
        PARSE_VALUE(asTYPEID_FLOAT, float, double);
        PARSE_VALUE(asTYPEID_DOUBLE, double, double);
        PARSE_VALUE(asTYPEID_BOOL, bool, bool);

#undef PARSE_VALUE

        return name_resolver.GetEnumValueName(prop->asObjType->GetName(), *(int*)&props->podData[prop->podDataOffset]);
    }
    else if (prop->dataType == Property::String)
    {
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));

        uchar* data = props->complexData[prop->complexDataIndex];
        uint data_size = props->complexDataSizes[prop->complexDataIndex];
        if (data_size)
            return string((char*)data, data_size);
        return string();
    }
    else if (prop->dataType == Property::Array)
    {
        if (prop->isArrayOfString)
        {
            if (data_size)
            {
                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(uint);
                DataBase::Array arr;
                arr.reserve(arr_size);
                for (uint i = 0; i < arr_size; i++)
                {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str((char*)data, str_size);
                    arr.push_back(std::move(str));
                    data += str_size;
                }
                return arr;
            }
            else
            {
                return DataBase::Array();
            }
        }
        else
        {
            int element_type_id = prop->asObjType->GetSubTypeId();
            uint element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(element_type_id);
            string element_type_name = prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            uint arr_size = data_size / element_size;
            DataBase::Array arr;
            arr.reserve(arr_size);
            for (uint i = 0; i < arr_size; i++)
            {
                if (prop->isHashSubType0)
                    arr.push_back(_str().parseHash(*(hash*)(data + i * element_size)).str());
                else if (element_type_id == asTYPEID_INT8)
                    arr.push_back((int)*(char*)(data + i * element_size));
                else if (element_type_id == asTYPEID_INT16)
                    arr.push_back((int)*(short*)(data + i * element_size));
                else if (element_type_id == asTYPEID_INT32)
                    arr.push_back((int)*(int*)(data + i * element_size));
                else if (element_type_id == asTYPEID_INT64)
                    arr.push_back((int64) * (int64*)(data + i * element_size));
                else if (element_type_id == asTYPEID_UINT8)
                    arr.push_back((int)*(uchar*)(data + i * element_size));
                else if (element_type_id == asTYPEID_UINT16)
                    arr.push_back((int)*(ushort*)(data + i * element_size));
                else if (element_type_id == asTYPEID_UINT32)
                    arr.push_back((int)*(uint*)(data + i * element_size));
                else if (element_type_id == asTYPEID_UINT64)
                    arr.push_back((int64) * (uint64*)(data + i * element_size));
                else if (element_type_id == asTYPEID_FLOAT)
                    arr.push_back((double)*(float*)(data + i * element_size));
                else if (element_type_id == asTYPEID_DOUBLE)
                    arr.push_back((double)*(double*)(data + i * element_size));
                else if (element_type_id == asTYPEID_BOOL)
                    arr.push_back((bool)*(bool*)(data + i * element_size));
                else if (!(element_type_id & asTYPEID_MASK_OBJECT) && element_type_id > asTYPEID_DOUBLE)
                    arr.push_back(name_resolver.GetEnumValueName(element_type_name, *(int*)(data + i * element_size)));
                else
                    throw UnreachablePlaceException(LINE_STR);
            }
            return arr;
        }
    }
    else if (prop->dataType == Property::Dict)
    {
        DataBase::Dict dict;
        if (data_size)
        {
            auto get_key_string = [&name_resolver](void* p, int type_id, string type_name, bool is_hash) -> string {
                if (is_hash)
                    return _str().parseHash(*(hash*)p).str();
                else if (type_id == asTYPEID_INT8)
                    return _str("{}", (int)*(char*)p).str();
                else if (type_id == asTYPEID_INT16)
                    return _str("{}", (int)*(short*)p).str();
                else if (type_id == asTYPEID_INT32)
                    return _str("{}", (int)*(int*)p).str();
                else if (type_id == asTYPEID_INT64)
                    return _str("{}", (int64) * (int64*)p).str();
                else if (type_id == asTYPEID_UINT8)
                    return _str("{}", (int)*(uchar*)p).str();
                else if (type_id == asTYPEID_UINT16)
                    return _str("{}", (int)*(ushort*)p).str();
                else if (type_id == asTYPEID_UINT32)
                    return _str("{}", (int)*(uint*)p).str();
                else if (type_id == asTYPEID_UINT64)
                    return _str("{}", (int64) * (uint64*)p).str();
                else if (type_id == asTYPEID_FLOAT)
                    return _str("{}", (double)*(float*)p).str();
                else if (type_id == asTYPEID_DOUBLE)
                    return _str("{}", (double)*(double*)p).str();
                else if (type_id == asTYPEID_BOOL)
                    return _str("{}", (bool)*(bool*)p).str();
                else if (!(type_id & asTYPEID_MASK_OBJECT) && type_id > asTYPEID_DOUBLE)
                    return name_resolver.GetEnumValueName(type_name, *(int*)p);
                else
                    throw UnreachablePlaceException(LINE_STR);
                return string();
            };

            int key_element_type_id = prop->asObjType->GetSubTypeId();
            string key_element_type_name =
                prop->asObjType->GetSubType() ? prop->asObjType->GetSubType()->GetName() : "";
            uint key_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(key_element_type_id);

            if (prop->isDictOfArray)
            {
                asITypeInfo* arr_type = prop->asObjType->GetSubType(1);
                int arr_element_type_id = arr_type->GetSubTypeId();
                string arr_element_type_name = arr_type->GetSubType() ? arr_type->GetSubType()->GetName() : "";
                uint arr_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(arr_element_type_id);
                uchar* data_end = data + data_size;
                while (data < data_end)
                {
                    void* key = data;
                    data += key_element_size;
                    uint arr_size;
                    std::memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(uint);
                    DataBase::Array arr;
                    arr.reserve(arr_size);
                    if (arr_size)
                    {
                        if (prop->isDictOfArrayOfString)
                        {
                            for (uint i = 0; i < arr_size; i++)
                            {
                                uint str_size;
                                std::memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(uint);
                                string str((char*)data, str_size);
                                arr.push_back(std::move(str));
                                data += str_size;
                            }
                        }
                        else
                        {
                            for (uint i = 0; i < arr_size; i++)
                            {
                                if (prop->isHashSubType2)
                                    arr.push_back(_str().parseHash(*(hash*)(data + i * arr_element_size)).str());
                                else if (arr_element_type_id == asTYPEID_INT8)
                                    arr.push_back((int)*(char*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_INT16)
                                    arr.push_back((int)*(short*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_INT32)
                                    arr.push_back((int)*(int*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_INT64)
                                    arr.push_back((int64) * (int64*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_UINT8)
                                    arr.push_back((int)*(uchar*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_UINT16)
                                    arr.push_back((int)*(ushort*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_UINT32)
                                    arr.push_back((int)*(uint*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_UINT64)
                                    arr.push_back((int64) * (uint64*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_FLOAT)
                                    arr.push_back((double)*(float*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_DOUBLE)
                                    arr.push_back((double)*(double*)(data + i * arr_element_size));
                                else if (arr_element_type_id == asTYPEID_BOOL)
                                    arr.push_back((bool)*(bool*)(data + i * arr_element_size));
                                else if (!(arr_element_type_id & asTYPEID_MASK_OBJECT) &&
                                    arr_element_type_id > asTYPEID_DOUBLE)
                                    arr.push_back(name_resolver.GetEnumValueName(
                                        arr_element_type_name, *(int*)(data + i * arr_element_size)));
                                else
                                    throw UnreachablePlaceException(LINE_STR);
                            }

                            data += arr_size * arr_element_size;
                        }
                    }

                    string key_str =
                        get_key_string(key, key_element_type_id, key_element_type_name, prop->isHashSubType0);
                    dict.insert(std::make_pair(std::move(key_str), std::move(arr)));
                }
            }
            else if (prop->isDictOfString)
            {
                uchar* data_end = data + data_size;
                while (data < data_end)
                {
                    void* key = data;
                    data += key_element_size;
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str((char*)data, str_size);
                    data += str_size;

                    string key_str =
                        get_key_string(key, key_element_type_id, key_element_type_name, prop->isHashSubType0);
                    dict.insert(std::make_pair(std::move(key_str), std::move(str)));
                }
            }
            else
            {
                int value_type_id = prop->asObjType->GetSubTypeId(1);
                uint value_element_size = prop->asObjType->GetEngine()->GetSizeOfPrimitiveType(value_type_id);
                string value_element_type_name =
                    prop->asObjType->GetSubType(1) ? prop->asObjType->GetSubType(1)->GetName() : "";
                uint whole_element_size = key_element_size + value_element_size;
                uint dict_size = data_size / whole_element_size;
                for (uint i = 0; i < dict_size; i++)
                {
                    uchar* pkey = data + i * whole_element_size;
                    uchar* pvalue = data + i * whole_element_size + key_element_size;
                    string key_str =
                        get_key_string(pkey, key_element_type_id, key_element_type_name, prop->isHashSubType0);

                    if (prop->isHashSubType1)
                        dict.insert(std::make_pair(std::move(key_str), _str().parseHash(*(hash*)pvalue).str()));
                    else if (value_type_id == asTYPEID_INT8)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(char*)pvalue));
                    else if (value_type_id == asTYPEID_INT16)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(short*)pvalue));
                    else if (value_type_id == asTYPEID_INT32)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(int*)pvalue));
                    else if (value_type_id == asTYPEID_INT64)
                        dict.insert(std::make_pair(std::move(key_str), (int64) * (int64*)pvalue));
                    else if (value_type_id == asTYPEID_UINT8)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(uchar*)pvalue));
                    else if (value_type_id == asTYPEID_UINT16)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(ushort*)pvalue));
                    else if (value_type_id == asTYPEID_UINT32)
                        dict.insert(std::make_pair(std::move(key_str), (int)*(uint*)pvalue));
                    else if (value_type_id == asTYPEID_UINT64)
                        dict.insert(std::make_pair(std::move(key_str), (int64) * (uint64*)pvalue));
                    else if (value_type_id == asTYPEID_FLOAT)
                        dict.insert(std::make_pair(std::move(key_str), (double)*(float*)pvalue));
                    else if (value_type_id == asTYPEID_DOUBLE)
                        dict.insert(std::make_pair(std::move(key_str), (double)*(double*)pvalue));
                    else if (value_type_id == asTYPEID_BOOL)
                        dict.insert(std::make_pair(std::move(key_str), (bool)*(bool*)pvalue));
                    else if (!(value_type_id & asTYPEID_MASK_OBJECT) && value_type_id > asTYPEID_DOUBLE)
                        dict.insert(std::make_pair(std::move(key_str),
                            name_resolver.GetEnumValueName(value_element_type_name, *(int*)pvalue)));
                    else
                        throw UnreachablePlaceException(LINE_STR);
                }
            }
        }
        return dict;
    }
    else
    {
        RUNTIME_ASSERT(!"Unexpected type");
    }*/
    return DataBase::Value();
}
