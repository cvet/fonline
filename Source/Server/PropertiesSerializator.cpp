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

auto PropertiesSerializator::SaveToDbDocument(const Properties* props, const Properties* base, NameResolver& name_resolver) -> DataBase::Document
{
    RUNTIME_ASSERT(!base || props->_registrator == base->_registrator);

    DataBase::Document doc;

    for (const auto* prop : props->_registrator->_registeredProperties) {
        // Skip pure virtual properties
        if (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1)) {
            continue;
        }

        // Skip temporary properties
        if (prop->_isTemporary) {
            continue;
        }

        // Skip same as in base
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

auto PropertiesSerializator::LoadFromDbDocument(Properties* props, const DataBase::Document& doc, NameResolver& name_resolver) -> bool
{
    bool is_error = false;

    for (const auto& [key, value] : doc) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_')
            continue;

        // Find property
        const auto* prop = props->_registrator->Find(key);

        if (!prop || (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1))) {
            if (!prop) {
                WriteLog("Unknown property '{}'.\n", key);
            }
            else {
                WriteLog("Invalid property '{}' for reading.\n", prop->GetName());
            }

            is_error = true;
            continue;
        }

        // Parse value
        if (prop->_dataType == Property::DataType::PlainData) {
            if (prop->_isHash) {
                if (value.index() != DataBase::STRING_VALUE) {
                    WriteLog("Wrong hash value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                const auto h = name_resolver.StringToHash(std::get<string>(value));
                props->SetRawData(prop, reinterpret_cast<const uchar*>(&h), prop->_baseSize);
            }
            else if (prop->_isEnum) {
                if (value.index() != DataBase::STRING_VALUE) {
                    WriteLog("Wrong enum value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                const auto e = name_resolver.ResolveEnumValue(prop->_baseTypeName, std::get<string>(value), is_error);
                props->SetRawData(prop, reinterpret_cast<const uchar*>(&e), prop->_baseSize);
            }
            else if (prop->_isInt || prop->_isFloat || prop->_isBool) {
                if (value.index() == DataBase::STRING_VALUE || value.index() == DataBase::ARRAY_VALUE || value.index() == DataBase::DICT_VALUE) {
                    WriteLog("Wrong integer value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

#define PARSE_VALUE(t) \
    do { \
        if (prop->_isInt8) { \
            *reinterpret_cast<char*>(pod_data) = static_cast<char>(std::get<t>(value)); \
        } \
        else if (prop->_isInt16) { \
            *reinterpret_cast<short*>(pod_data) = static_cast<short>(std::get<t>(value)); \
        } \
        else if (prop->_isInt32) { \
            *reinterpret_cast<int*>(pod_data) = static_cast<int>(std::get<t>(value)); \
        } \
        else if (prop->_isInt64) { \
            *reinterpret_cast<int64*>(pod_data) = static_cast<int64>(std::get<t>(value)); \
        } \
        else if (prop->_isUInt8) { \
            *reinterpret_cast<uchar*>(pod_data) = static_cast<uchar>(std::get<t>(value)); \
        } \
        else if (prop->_isUInt16) { \
            *reinterpret_cast<short*>(pod_data) = static_cast<short>(std::get<t>(value)); \
        } \
        else if (prop->_isUInt32) { \
            *reinterpret_cast<uint*>(pod_data) = static_cast<uint>(std::get<t>(value)); \
        } \
        else if (prop->_isUInt64) { \
            *reinterpret_cast<uint64*>(pod_data) = static_cast<uint64>(std::get<t>(value)); \
        } \
        else if (prop->_isSingleFloat) { \
            *reinterpret_cast<float*>(pod_data) = static_cast<float>(std::get<t>(value)); \
        } \
        else if (prop->_isDoubleFloat) { \
            *reinterpret_cast<double*>(pod_data) = static_cast<double>(std::get<t>(value)); \
        } \
        else if (prop->_isBool) { \
            *reinterpret_cast<bool*>(pod_data) = (std::get<t>(value) != static_cast<t>(0)); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                uchar pod_data[8];
                if (value.index() == DataBase::INT_VALUE) {
                    PARSE_VALUE(int);
                }
                else if (value.index() == DataBase::INT64_VALUE) {
                    PARSE_VALUE(int64);
                }
                else if (value.index() == DataBase::DOUBLE_VALUE) {
                    PARSE_VALUE(double);
                }
                else if (value.index() == DataBase::BOOL_VALUE) {
                    PARSE_VALUE(bool);
                }
                else {
                    throw UnreachablePlaceException(LINE_STR);
                }

#undef PARSE_VALUE

                props->SetRawData(prop, pod_data, prop->_baseSize);
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }
        }
        else if (prop->_dataType == Property::DataType::String) {
            if (value.index() != DataBase::STRING_VALUE) {
                WriteLog("Wrong string value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            const auto& str = std::get<string>(value);

            props->SetRawData(prop, reinterpret_cast<const uchar*>(str.c_str()), static_cast<uint>(str.length()));
        }
        else if (prop->_dataType == Property::DataType::Array) {
            if (value.index() != DataBase::ARRAY_VALUE) {
                WriteLog("Wrong array value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            const auto& arr = std::get<DataBase::Array>(value);

            if (arr.empty()) {
                props->SetRawData(prop, nullptr, 0u);
                continue;
            }

            if (prop->_isHash) {
                if (arr[0].index() != DataBase::STRING_VALUE) {
                    WriteLog("Wrong array hash element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = static_cast<uint>(arr.size()) * sizeof(hash);
                auto* data = new uchar[data_size];

                for (size_t i = 0; i < arr.size(); i++) {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::STRING_VALUE);

                    const auto h = name_resolver.StringToHash(std::get<string>(arr[i]));
                    *reinterpret_cast<hash*>(data + i * sizeof(hash)) = h;
                }

                props->SetRawData(prop, data, data_size);

                delete[] data;
            }
            else if (prop->_isEnum) {
                if (arr[0].index() != DataBase::STRING_VALUE) {
                    WriteLog("Wrong array enum element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = static_cast<uint>(arr.size()) * prop->_baseSize;
                auto* data = new uchar[data_size];

                for (size_t i = 0; i < arr.size(); i++) {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::STRING_VALUE);

                    int e = name_resolver.ResolveEnumValue(prop->_baseTypeName, std::get<string>(arr[i]), is_error);
                    memcpy(data + i * prop->_baseSize, &e, prop->_baseSize);
                }

                props->SetRawData(prop, data, data_size);

                delete[] data;
            }
            else if (prop->_isInt || prop->_isFloat || prop->_isBool) {
                if (arr[0].index() == DataBase::STRING_VALUE || arr[0].index() == DataBase::ARRAY_VALUE || arr[0].index() == DataBase::DICT_VALUE) {
                    WriteLog("Wrong array element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = prop->_baseSize * static_cast<uint>(arr.size());
                auto* data = new uchar[data_size];
                const auto arr_element_index = arr[0].index();

#define PARSE_VALUE(t) \
    do { \
        for (size_t i = 0; i < arr.size(); i++) { \
            RUNTIME_ASSERT(arr[i].index() == arr_element_index); \
            if (arr_element_index == DataBase::INT_VALUE) { \
                *reinterpret_cast<t*>(data + i * prop->_baseSize) = static_cast<t>(std::get<int>(arr[i])); \
            } \
            else if (arr_element_index == DataBase::INT64_VALUE) { \
                *reinterpret_cast<t*>(data + i * prop->_baseSize) = static_cast<t>(std::get<int64>(arr[i])); \
            } \
            else if (arr_element_index == DataBase::DOUBLE_VALUE) { \
                *reinterpret_cast<t*>(data + i * prop->_baseSize) = static_cast<t>(std::get<double>(arr[i])); \
            } \
            else if (arr_element_index == DataBase::BOOL_VALUE) { \
                *reinterpret_cast<t*>(data + i * prop->_baseSize) = static_cast<t>(std::get<bool>(arr[i])); \
            } \
            else { \
                throw UnreachablePlaceException(LINE_STR); \
            } \
        } \
    } while (false)

                if (prop->_isInt8) {
                    PARSE_VALUE(char);
                }
                else if (prop->_isInt16) {
                    PARSE_VALUE(short);
                }
                else if (prop->_isInt32) {
                    PARSE_VALUE(int);
                }
                else if (prop->_isInt64) {
                    PARSE_VALUE(int64);
                }
                else if (prop->_isUInt8) {
                    PARSE_VALUE(uchar);
                }
                else if (prop->_isUInt16) {
                    PARSE_VALUE(ushort);
                }
                else if (prop->_isUInt32) {
                    PARSE_VALUE(uint);
                }
                else if (prop->_isUInt64) {
                    PARSE_VALUE(uint64);
                }
                else if (prop->_isSingleFloat) {
                    PARSE_VALUE(float);
                }
                else if (prop->_isDoubleFloat) {
                    PARSE_VALUE(double);
                }
                else if (prop->_isBool) {
                    PARSE_VALUE(bool);
                }
                else {
                    throw UnreachablePlaceException(LINE_STR);
                }

#undef PARSE_VALUE

                props->SetRawData(prop, data, data_size);

                delete[] data;
            }
            else {
                RUNTIME_ASSERT(prop->_isArrayOfString);

                if (arr[0].index() != DataBase::STRING_VALUE) {
                    WriteLog("Wrong array element value type, property '{}'.\n", prop->GetName());
                    is_error = true;
                    continue;
                }

                uint data_size = sizeof(uint);
                for (size_t i = 0; i < arr.size(); i++) {
                    RUNTIME_ASSERT(arr[i].index() == DataBase::STRING_VALUE);

                    string_view str = std::get<string>(arr[i]);
                    data_size += sizeof(uint) + static_cast<uint>(str.length());
                }

                auto* data = new uchar[data_size];
                *reinterpret_cast<uint*>(data) = static_cast<uint>(arr.size());

                size_t data_pos = sizeof(uint);
                for (size_t i = 0; i < arr.size(); i++) {
                    const auto& str = std::get<string>(arr[i]);
                    *reinterpret_cast<uint*>(data + data_pos) = static_cast<uint>(str.length());
                    if (!str.empty()) {
                        std::memcpy(data + data_pos + sizeof(uint), str.c_str(), str.length());
                    }
                    data_pos += sizeof(uint) + str.length();
                }
                RUNTIME_ASSERT(data_pos == data_size);

                props->SetRawData(prop, data, data_size);

                delete[] data;
            }
        }
        else if (prop->_dataType == Property::DataType::Dict) {
            if (value.index() != DataBase::DICT_VALUE) {
                WriteLog("Wrong dict value type, property '{}'.\n", prop->GetName());
                is_error = true;
                continue;
            }

            const auto& dict = std::get<DataBase::Dict>(value);

            if (dict.empty()) {
                props->SetRawData(prop, nullptr, 0u);
                continue;
            }

            // Measure data length
            uint data_size = 0;
            bool wrong_input = false;
            for (const auto& [key2, value2] : dict) {
                data_size += prop->_dictKeySize;

                if (prop->_isDictOfArray) {
                    if (value2.index() != DataBase::ARRAY_VALUE) {
                        WriteLog("Wrong dict array value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    const auto& arr = std::get<DataBase::Array>(value2);

                    data_size += sizeof(uint);

                    if (prop->_isDictKeyHash) {
                        for (const auto& e : arr) {
                            if (e.index() != DataBase::STRING_VALUE) {
                                WriteLog("Wrong dict array element hash value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += static_cast<uint>(arr.size()) * sizeof(hash);
                    }
                    else if (prop->_isDictKeyEnum) {
                        for (const auto& e : arr) {
                            if (e.index() != DataBase::STRING_VALUE) {
                                WriteLog("Wrong dict array element enum value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += static_cast<uint>(arr.size()) * prop->_baseSize;
                    }
                    else if (prop->_isDictOfArrayOfString) {
                        for (const auto& e : arr) {
                            if (e.index() != DataBase::STRING_VALUE) {
                                WriteLog("Wrong dict array element string value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }

                            data_size += sizeof(uint) + static_cast<uint>(std::get<string>(e).length());
                        }
                    }
                    else {
                        for (const auto& e : arr) {
                            if ((e.index() != DataBase::INT_VALUE && e.index() != DataBase::INT64_VALUE && e.index() != DataBase::DOUBLE_VALUE && e.index() != DataBase::BOOL_VALUE) || e.index() != arr[0].index()) {
                                WriteLog("Wrong dict array element value type, property '{}'.\n", prop->GetName());
                                wrong_input = true;
                                break;
                            }
                        }

                        data_size += static_cast<int>(arr.size()) * prop->_baseSize;
                    }
                }
                else if (prop->_isDictOfString) {
                    if (value2.index() != DataBase::STRING_VALUE) {
                        WriteLog("Wrong dict string element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += static_cast<uint>(std::get<string>(value2).length());
                }
                else if (prop->_isHash) {
                    if (value2.index() != DataBase::STRING_VALUE) {
                        WriteLog("Wrong dict hash/resource element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += sizeof(hash);
                }
                else if (prop->_isEnum) {
                    if (value2.index() != DataBase::STRING_VALUE) {
                        WriteLog("Wrong dict enum element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += prop->_baseSize;
                }
                else {
                    if (value2.index() != DataBase::INT_VALUE && value2.index() != DataBase::INT64_VALUE && value2.index() != DataBase::DOUBLE_VALUE && value2.index() != DataBase::BOOL_VALUE) {
                        WriteLog("Wrong dict number element value type, property '{}'.\n", prop->GetName());
                        wrong_input = true;
                        break;
                    }

                    data_size += prop->_baseSize;
                }

                if (wrong_input) {
                    break;
                }
            }

            if (wrong_input) {
                is_error = true;
                continue;
            }

            // Write data
            auto* data = new uchar[data_size];
            size_t data_pos = 0;

            for (const auto& [key2, value2] : dict) {
                // Key
                if (prop->_isDictKeyHash) {
                    *reinterpret_cast<hash*>(data + data_pos) = name_resolver.StringToHash(key2);
                }
                else if (prop->_isDictKeyEnum) {
                    int enum_value = name_resolver.ResolveEnumValue(prop->_dictKeyTypeName, key2, is_error);
                    memcpy(data + data_pos, &enum_value, prop->_baseSize);
                }
                else if (prop->_dictKeySize == 1u) {
                    *reinterpret_cast<char*>(data + data_pos) = static_cast<char>(_str(key2).toInt64());
                }
                else if (prop->_dictKeySize == 2u) {
                    *reinterpret_cast<short*>(data + data_pos) = static_cast<short>(_str(key2).toInt64());
                }
                else if (prop->_dictKeySize == 4u) {
                    *reinterpret_cast<int*>(data + data_pos) = static_cast<int>(_str(key2).toInt64());
                }
                else if (prop->_dictKeySize == 8u) {
                    *reinterpret_cast<int64*>(data + data_pos) = _str(key2).toInt64();
                }
                else {
                    throw UnreachablePlaceException(LINE_STR);
                }

                data_pos += prop->_dictKeySize;

                // Value
                if (prop->_isDictOfArray) {
                    const auto& arr = std::get<DataBase::Array>(value2);

                    *reinterpret_cast<uint*>(data + data_pos) = static_cast<uint>(arr.size());
                    data_pos += sizeof(uint);

                    if (prop->_isEnum) {
                        for (const auto& e : arr) {
                            const int enum_value = name_resolver.ResolveEnumValue(prop->_baseTypeName, std::get<string>(e), is_error);
                            memcpy(data + data_pos, &enum_value, prop->_baseSize);
                            data_pos += prop->_baseSize;
                        }
                    }
                    else if (prop->_isHash) {
                        for (const auto& e : arr) {
                            *reinterpret_cast<hash*>(data + data_pos) = name_resolver.StringToHash(std::get<string>(e));
                            data_pos += sizeof(hash);
                        }
                    }
                    else if (prop->_isDictOfArrayOfString) {
                        for (const auto& e : arr) {
                            const auto& str = std::get<string>(e);
                            *reinterpret_cast<uint*>(data + data_pos) = static_cast<uint>(str.length());
                            data_pos += sizeof(uint);
                            if (!str.empty()) {
                                std::memcpy(data + data_pos, str.c_str(), str.length());
                                data_pos += static_cast<uint>(str.length());
                            }
                        }
                    }
                    else {
                        for (const auto& e : arr) {
#define PARSE_VALUE(t) \
    do { \
        RUNTIME_ASSERT(sizeof(t) == prop->_baseSize); \
        if (e.index() == DataBase::INT_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<int>(e)); \
        } \
        else if (e.index() == DataBase::INT64_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<int64>(e)); \
        } \
        else if (e.index() == DataBase::DOUBLE_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<double>(e)); \
        } \
        else if (e.index() == DataBase::BOOL_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<bool>(e)); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                            if (prop->_isInt8) {
                                PARSE_VALUE(char);
                            }
                            else if (prop->_isInt16) {
                                PARSE_VALUE(short);
                            }
                            else if (prop->_isInt32) {
                                PARSE_VALUE(int);
                            }
                            else if (prop->_isInt64) {
                                PARSE_VALUE(int64);
                            }
                            else if (prop->_isUInt8) {
                                PARSE_VALUE(uchar);
                            }
                            else if (prop->_isUInt16) {
                                PARSE_VALUE(ushort);
                            }
                            else if (prop->_isUInt32) {
                                PARSE_VALUE(uint);
                            }
                            else if (prop->_isUInt64) {
                                PARSE_VALUE(uint64);
                            }
                            else if (prop->_isSingleFloat) {
                                PARSE_VALUE(float);
                            }
                            else if (prop->_isDoubleFloat) {
                                PARSE_VALUE(double);
                            }
                            else if (prop->_isBool) {
                                PARSE_VALUE(bool);
                            }
                            else {
                                throw UnreachablePlaceException(LINE_STR);
                            }

#undef PARSE_VALUE

                            data_pos += prop->_baseSize;
                        }
                    }
                }
                else if (prop->_isDictOfString) {
                    const auto& str = std::get<string>(value2);

                    *reinterpret_cast<uint*>(data + data_pos) = static_cast<uint>(str.length());
                    data_pos += sizeof(uint);

                    if (!str.empty()) {
                        std::memcpy(data + data_pos, str.c_str(), str.length());
                        data_pos += str.length();
                    }
                }
                else {
                    if (prop->_isHash) {
                        *reinterpret_cast<hash*>(data + data_pos) = name_resolver.StringToHash(std::get<string>(value2));
                    }
                    else if (prop->_isEnum) {
                        const int enum_value = name_resolver.ResolveEnumValue(prop->_baseTypeName, std::get<string>(value2), is_error);
                        memcpy(data + data_pos, &enum_value, prop->_baseSize);
                    }
                    else {
#define PARSE_VALUE(t) \
    do { \
        RUNTIME_ASSERT(sizeof(t) == prop->_baseSize); \
        if (value2.index() == DataBase::INT_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<int>(value2)); \
        } \
        else if (value2.index() == DataBase::INT64_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<int64>(value2)); \
        } \
        else if (value2.index() == DataBase::DOUBLE_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<double>(value2)); \
        } \
        else if (value2.index() == DataBase::BOOL_VALUE) { \
            *reinterpret_cast<t*>(data + data_pos) = static_cast<t>(std::get<bool>(value2)); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                        if (prop->_isInt8) {
                            PARSE_VALUE(char);
                        }
                        else if (prop->_isInt16) {
                            PARSE_VALUE(short);
                        }
                        else if (prop->_isInt32) {
                            PARSE_VALUE(int);
                        }
                        else if (prop->_isInt64) {
                            PARSE_VALUE(int64);
                        }
                        else if (prop->_isUInt8) {
                            PARSE_VALUE(uchar);
                        }
                        else if (prop->_isUInt16) {
                            PARSE_VALUE(ushort);
                        }
                        else if (prop->_isUInt32) {
                            PARSE_VALUE(uint);
                        }
                        else if (prop->_isUInt64) {
                            PARSE_VALUE(uint64);
                        }
                        else if (prop->_isSingleFloat) {
                            PARSE_VALUE(float);
                        }
                        else if (prop->_isDoubleFloat) {
                            PARSE_VALUE(double);
                        }
                        else if (prop->_isBool) {
                            PARSE_VALUE(bool);
                        }
                        else {
                            throw UnreachablePlaceException(LINE_STR);
                        }

#undef PARSE_VALUE
                    }

                    data_pos += prop->_baseSize;
                }
            }

            if (data_pos != data_size) {
                WriteLog("{} {}\n", data_pos, data_size);
            }

            RUNTIME_ASSERT(data_pos == data_size);

            props->SetRawData(prop, data, data_size);

            delete[] data;
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }
    return !is_error;
}

auto PropertiesSerializator::SavePropertyToDbValue(const Properties* props, const Property* prop, NameResolver& name_resolver) -> DataBase::Value
{
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1) || prop->_complexDataIndex != static_cast<uint>(-1));
    RUNTIME_ASSERT(!prop->_isTemporary);

    uint data_size;
    const auto* data = props->GetRawData(prop, data_size);

    if (prop->_dataType == Property::DataType::PlainData) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));

        if (prop->_isHash) {
            return string(name_resolver.HashToString(*reinterpret_cast<hash*>(&props->_podData[prop->_podDataOffset])));
        }
        else if (prop->_isEnum) {
            int enum_value = 0;
            memcpy(&enum_value, &props->_podData[prop->_podDataOffset], prop->_baseSize);
            return name_resolver.ResolveEnumValueName(prop->_baseTypeName, enum_value);
        }
        else if (prop->_isInt || prop->_isFloat || prop->_isBool) {
#define PARSE_VALUE(is, t, ret_t) \
    do { \
        if (prop->is) { \
            return static_cast<ret_t>(*reinterpret_cast<t*>(&props->_podData[prop->_podDataOffset])); \
        } \
    } while (false)

            PARSE_VALUE(_isInt8, char, int);
            PARSE_VALUE(_isInt16, short, int);
            PARSE_VALUE(_isInt32, int, int);
            PARSE_VALUE(_isInt64, int64, int64);
            PARSE_VALUE(_isUInt8, uchar, int);
            PARSE_VALUE(_isUInt16, ushort, int);
            PARSE_VALUE(_isUInt32, uint, int);
            PARSE_VALUE(_isUInt64, uint64, int64);
            PARSE_VALUE(_isSingleFloat, float, double);
            PARSE_VALUE(_isDoubleFloat, double, double);
            PARSE_VALUE(_isBool, bool, bool);

#undef PARSE_VALUE
        }
    }
    else if (prop->_dataType == Property::DataType::String) {
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));

        if (data_size > 0u) {
            return string(reinterpret_cast<const char*>(data), data_size);
        }

        return string();
    }
    else if (prop->_dataType == Property::DataType::Array) {
        if (prop->_isArrayOfString) {
            if (data_size > 0u) {
                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(uint);

                DataBase::Array arr;
                arr.reserve(arr_size);
                for (uint i = 0; i < arr_size; i++) {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr.push_back(std::move(str));
                    data += str_size;
                }

                return arr;
            }

            return DataBase::Array();
        }
        else {
            uint arr_size = data_size / prop->_baseSize;

            DataBase::Array arr;
            arr.reserve(arr_size);

            for (uint i = 0; i < arr_size; i++) {
                if (prop->_isHash) {
                    arr.push_back(string(name_resolver.HashToString(*reinterpret_cast<const hash*>(data + i * prop->_baseSize))));
                }
                else if (prop->_isEnum) {
                    int enum_value = 0;
                    memcpy(&enum_value, data + i * prop->_baseSize, prop->_baseSize);
                    arr.push_back(name_resolver.ResolveEnumValueName(prop->_baseTypeName, enum_value));
                }
                else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->_baseSize); \
    arr.push_back(static_cast<db_t>(*reinterpret_cast<const t*>(data + i * prop->_baseSize)))

                    if (prop->_isInt8) {
                        PARSE_VALUE(char, int);
                    }
                    else if (prop->_isInt16) {
                        PARSE_VALUE(short, int);
                    }
                    else if (prop->_isInt32) {
                        PARSE_VALUE(int, int);
                    }
                    else if (prop->_isInt64) {
                        PARSE_VALUE(int64, int64);
                    }
                    else if (prop->_isUInt8) {
                        PARSE_VALUE(uchar, int);
                    }
                    else if (prop->_isUInt16) {
                        PARSE_VALUE(ushort, int);
                    }
                    else if (prop->_isUInt32) {
                        PARSE_VALUE(uint, int);
                    }
                    else if (prop->_isUInt64) {
                        PARSE_VALUE(uint64, int64);
                    }
                    else if (prop->_isSingleFloat) {
                        PARSE_VALUE(float, double);
                    }
                    else if (prop->_isDoubleFloat) {
                        PARSE_VALUE(double, double);
                    }
                    else if (prop->_isBool) {
                        PARSE_VALUE(bool, bool);
                    }
                    else {
                        throw UnreachablePlaceException(LINE_STR);
                    }

#undef PARSE_VALUE
                }
            }

            return arr;
        }
    }
    else if (prop->_dataType == Property::DataType::Dict) {
        DataBase::Dict dict;

        if (data_size > 0u) {
            const auto get_key_string = [prop, &name_resolver](const uchar* p) -> string {
                if (prop->_isDictKeyHash) {
                    return string(name_resolver.HashToString(*reinterpret_cast<const hash*>(p)));
                }
                else if (prop->_isDictKeyEnum) {
                    int enum_value = 0;
                    memcpy(&enum_value, p, prop->_dictKeySize);
                    return name_resolver.ResolveEnumValueName(prop->_dictKeyTypeName, enum_value);
                }
                else if (prop->_dictKeySize == 1u) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const char*>(p))).str();
                }
                else if (prop->_dictKeySize == 2u) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const short*>(p))).str();
                }
                else if (prop->_dictKeySize == 4u) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const int*>(p))).str();
                }
                else if (prop->_dictKeySize == 8u) {
                    return _str("{}", static_cast<int64>(*reinterpret_cast<const int64*>(p))).str();
                }
                throw UnreachablePlaceException(LINE_STR);
            };

            if (prop->_isDictOfArray) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += prop->_dictKeySize;

                    uint arr_size;
                    std::memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(uint);

                    DataBase::Array arr;
                    arr.reserve(arr_size);

                    if (arr_size > 0u) {
                        if (prop->_isDictOfArrayOfString) {
                            for (uint i = 0; i < arr_size; i++) {
                                uint str_size;
                                std::memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(uint);
                                string str(reinterpret_cast<const char*>(data), str_size);
                                arr.push_back(std::move(str));
                                data += str_size;
                            }
                        }
                        else {
                            for (uint i = 0; i < arr_size; i++) {
                                if (prop->_isHash) {
                                    arr.push_back(string(name_resolver.HashToString(*reinterpret_cast<const hash*>(data + i * sizeof(hash)))));
                                }
                                else if (prop->_isEnum) {
                                    int enum_value = 0;
                                    memcpy(&enum_value, data + i * prop->_baseSize, prop->_baseSize);
                                    arr.push_back(name_resolver.ResolveEnumValueName(prop->_baseTypeName, enum_value));
                                }
                                else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->_baseSize); \
    arr.push_back(static_cast<db_t>(*reinterpret_cast<const t*>(data + i * prop->_baseSize)))

                                    if (prop->_isInt8) {
                                        PARSE_VALUE(char, int);
                                    }
                                    else if (prop->_isInt16) {
                                        PARSE_VALUE(short, int);
                                    }
                                    else if (prop->_isInt32) {
                                        PARSE_VALUE(int, int);
                                    }
                                    else if (prop->_isInt64) {
                                        PARSE_VALUE(int64, int64);
                                    }
                                    else if (prop->_isUInt8) {
                                        PARSE_VALUE(uchar, int);
                                    }
                                    else if (prop->_isUInt16) {
                                        PARSE_VALUE(ushort, int);
                                    }
                                    else if (prop->_isUInt32) {
                                        PARSE_VALUE(uint, int);
                                    }
                                    else if (prop->_isUInt64) {
                                        PARSE_VALUE(uint64, int64);
                                    }
                                    else if (prop->_isSingleFloat) {
                                        PARSE_VALUE(float, double);
                                    }
                                    else if (prop->_isDoubleFloat) {
                                        PARSE_VALUE(double, double);
                                    }
                                    else if (prop->_isBool) {
                                        PARSE_VALUE(bool, bool);
                                    }
                                    else {
                                        throw UnreachablePlaceException(LINE_STR);
                                    }

#undef PARSE_VALUE
                                }
                            }

                            data += arr_size * prop->_baseSize;
                        }
                    }

                    string key_str = get_key_string(key);
                    dict.insert(std::make_pair(std::move(key_str), std::move(arr)));
                }
            }
            else if (prop->_isDictOfString) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += prop->_dictKeySize;

                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    data += str_size;

                    auto key_str = get_key_string(key);
                    dict.insert(std::make_pair(std::move(key_str), std::move(str)));
                }
            }
            else {
                uint whole_element_size = prop->_dictKeySize + prop->_baseSize;
                uint dict_size = data_size / whole_element_size;

                for (uint i = 0; i < dict_size; i++) {
                    const auto* pkey = data + i * whole_element_size;
                    const auto* pvalue = data + i * whole_element_size + prop->_dictKeySize;

                    string key_str = get_key_string(pkey);

                    if (prop->_isHash) {
                        dict.insert(std::make_pair(std::move(key_str), string(name_resolver.HashToString(*reinterpret_cast<const hash*>(pvalue)))));
                    }
                    else if (prop->_isEnum) {
                        int enum_value = 0;
                        memcpy(&enum_value, pvalue, prop->_baseSize);
                        dict.insert(std::make_pair(std::move(key_str), name_resolver.ResolveEnumValueName(prop->_baseTypeName, enum_value)));
                    }
                    else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->_baseSize); \
    dict.insert(std::make_pair(std::move(key_str), static_cast<db_t>(*reinterpret_cast<const t*>(pvalue))))

                        if (prop->_isInt8) {
                            PARSE_VALUE(char, int);
                        }
                        else if (prop->_isInt16) {
                            PARSE_VALUE(short, int);
                        }
                        else if (prop->_isInt32) {
                            PARSE_VALUE(int, int);
                        }
                        else if (prop->_isInt64) {
                            PARSE_VALUE(int64, int64);
                        }
                        else if (prop->_isUInt8) {
                            PARSE_VALUE(uchar, int);
                        }
                        else if (prop->_isUInt16) {
                            PARSE_VALUE(ushort, int);
                        }
                        else if (prop->_isUInt32) {
                            PARSE_VALUE(uint, int);
                        }
                        else if (prop->_isUInt64) {
                            PARSE_VALUE(uint64, int64);
                        }
                        else if (prop->_isSingleFloat) {
                            PARSE_VALUE(float, double);
                        }
                        else if (prop->_isDoubleFloat) {
                            PARSE_VALUE(double, double);
                        }
                        else if (prop->_isBool) {
                            PARSE_VALUE(bool, bool);
                        }
                        else {
                            throw UnreachablePlaceException(LINE_STR);
                        }

#undef PARSE_VALUE
                    }
                }
            }
        }

        return dict;
    }

    throw UnreachablePlaceException(LINE_STR);
}
