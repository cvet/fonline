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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

auto PropertiesSerializator::SaveToDocument(const Properties* props, const Properties* base, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Document
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!base || props->GetRegistrator() == base->GetRegistrator());

    AnyData::Document doc;

    for (const auto* prop : props->GetRegistrator()->GetProperties()) {
        if (prop->IsDisabled()) {
            continue;
        }
        if (prop->IsVirtual()) {
            continue;
        }
        if (prop->IsTemporary()) {
            continue;
        }

        // Skip same as in base
        if (base != nullptr) {
            uint base_data_size;
            const auto* base_data = base->GetRawData(prop, base_data_size);

            uint data_size;
            const auto* data = props->GetRawData(prop, data_size);

            if (data_size == base_data_size && std::memcmp(data, base_data, data_size) == 0) {
                continue;
            }
        }
        else {
            uint data_size;
            const auto* data = props->GetRawData(prop, data_size);

            if (prop->IsPlainData()) {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(data_size <= sizeof(pod_zero));
                if (std::memcmp(data, &pod_zero, data_size) == 0) {
                    continue;
                }
            }
            else {
                if (data_size == 0) {
                    continue;
                }
            }
        }

        doc.insert(std::make_pair(prop->GetName(), SavePropertyToValue(props, prop, hash_resolver, name_resolver)));
    }

    return doc;
}

auto PropertiesSerializator::LoadFromDocument(Properties* props, const AnyData::Document& doc, HashResolver& hash_resolver, NameResolver& name_resolver) -> bool
{
    STACK_TRACE_ENTRY();

    bool is_error = false;

    for (const auto& [key, value] : doc) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        // Find property
        const auto* prop = props->GetRegistrator()->Find(key);

        if (prop != nullptr && !prop->IsDisabled() && !prop->IsVirtual() && !prop->IsTemporary()) {
            if (!LoadPropertyFromValue(props, prop, value, hash_resolver, name_resolver)) {
                is_error = true;
            }
        }
        else {
            // Todo: maybe need some optional warning for unknown/wrong properties
            // WriteLog("Skip unknown property {}", key);
        }
    }

    return !is_error;
}

auto PropertiesSerializator::SavePropertyToValue(const Properties* props, const Property* prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!prop->IsDisabled());
    RUNTIME_ASSERT(!prop->IsVirtual());
    RUNTIME_ASSERT(!prop->IsTemporary());

    uint data_size;
    const auto* data = props->GetRawData(prop, data_size);

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            const auto hash = *reinterpret_cast<const hstring::hash_t*>(data);
            return string {hash_resolver.ResolveHash(hash)};
        }
        else if (prop->IsBaseTypeEnum()) {
            int enum_value = 0;
            std::memcpy(&enum_value, data, prop->GetBaseSize());
            return name_resolver.ResolveEnumValueName(prop->GetBaseTypeName(), enum_value);
        }
        else if (prop->IsInt() || prop->IsFloat() || prop->IsBool()) {
#define PARSE_VALUE(is, t, ret_t) \
    do { \
        if (prop->is) { \
            return static_cast<ret_t>(*static_cast<const t*>(reinterpret_cast<const void*>(data))); \
        } \
    } while (false)

            PARSE_VALUE(IsInt8(), int8, int);
            PARSE_VALUE(IsInt16(), int16, int);
            PARSE_VALUE(IsInt32(), int, int);
            PARSE_VALUE(IsInt64(), int64, int64);
            PARSE_VALUE(IsUInt8(), uint8, int);
            PARSE_VALUE(IsUInt16(), uint16, int);
            PARSE_VALUE(IsUInt32(), uint, int);
            PARSE_VALUE(IsUInt64(), uint64, int64);
            PARSE_VALUE(IsSingleFloat(), float, double);
            PARSE_VALUE(IsDoubleFloat(), double, double);
            PARSE_VALUE(IsBool(), bool, bool);

#undef PARSE_VALUE
        }
    }
    else if (prop->IsString()) {
        if (data_size != 0) {
            return string {reinterpret_cast<const char*>(data), data_size};
        }

        return string {};
    }
    else if (prop->IsArray()) {
        if (prop->IsArrayOfString()) {
            if (data_size != 0) {
                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(uint);

                AnyData::Array arr;
                arr.reserve(arr_size);
                for (uint i = 0; i < arr_size; i++) {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr.emplace_back(std::move(str));
                    data += str_size;
                }

                return arr;
            }

            return AnyData::Array();
        }
        else {
            uint arr_size = data_size / prop->GetBaseSize();

            AnyData::Array arr;
            arr.reserve(arr_size);

            for (uint i = 0; i < arr_size; i++) {
                if (prop->IsBaseTypeHash()) {
                    const auto hash = *reinterpret_cast<const hstring::hash_t*>(data + static_cast<size_t>(i) * prop->GetBaseSize());
                    arr.emplace_back(string {hash_resolver.ResolveHash(hash)});
                }
                else if (prop->IsBaseTypeEnum()) {
                    int enum_value = 0;
                    std::memcpy(&enum_value, data + static_cast<size_t>(i) * prop->GetBaseSize(), prop->GetBaseSize());
                    arr.emplace_back(name_resolver.ResolveEnumValueName(prop->GetBaseTypeName(), enum_value));
                }
                else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->GetBaseSize()); \
    arr.push_back(static_cast<db_t>(*static_cast<const t*>(reinterpret_cast<const void*>(data + i * prop->GetBaseSize()))))

                    if (prop->IsInt8()) {
                        PARSE_VALUE(int8, int);
                    }
                    else if (prop->IsInt16()) {
                        PARSE_VALUE(int16, int);
                    }
                    else if (prop->IsInt32()) {
                        PARSE_VALUE(int, int);
                    }
                    else if (prop->IsInt64()) {
                        PARSE_VALUE(int64, int64);
                    }
                    else if (prop->IsUInt8()) {
                        PARSE_VALUE(uint8, int);
                    }
                    else if (prop->IsUInt16()) {
                        PARSE_VALUE(uint16, int);
                    }
                    else if (prop->IsUInt32()) {
                        PARSE_VALUE(uint, int);
                    }
                    else if (prop->IsUInt64()) {
                        PARSE_VALUE(uint64, int64);
                    }
                    else if (prop->IsSingleFloat()) {
                        PARSE_VALUE(float, double);
                    }
                    else if (prop->IsDoubleFloat()) {
                        PARSE_VALUE(double, double);
                    }
                    else if (prop->IsBool()) {
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
    else if (prop->IsDict()) {
        AnyData::Dict dict;

        if (data_size != 0) {
            const auto get_key_string = [prop, &hash_resolver, &name_resolver](const uint8* p) -> string {
                if (prop->IsDictKeyString()) {
                    const uint str_len = *reinterpret_cast<const uint*>(p);
                    return string {reinterpret_cast<const char*>(p + sizeof(uint)), str_len};
                }
                else if (prop->IsDictKeyHash()) {
                    const auto hash = *reinterpret_cast<const hstring::hash_t*>(p);
                    return hash_resolver.ResolveHash(hash).as_str();
                }
                else if (prop->IsDictKeyEnum()) {
                    int enum_value = 0;
                    std::memcpy(&enum_value, p, prop->GetDictKeySize());
                    return name_resolver.ResolveEnumValueName(prop->GetDictKeyTypeName(), enum_value);
                }
                else if (prop->GetDictKeySize() == 1) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const int8*>(p))).str();
                }
                else if (prop->GetDictKeySize() == 2) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const int16*>(p))).str();
                }
                else if (prop->GetDictKeySize() == 4) {
                    return _str("{}", static_cast<int>(*reinterpret_cast<const int*>(p))).str();
                }
                else if (prop->GetDictKeySize() == 8) {
                    return _str("{}", static_cast<int64>(*reinterpret_cast<const int64*>(p))).str();
                }
                throw UnreachablePlaceException(LINE_STR);
            };

            const auto get_key_len = [prop](const uint8* p) -> size_t {
                if (prop->IsDictKeyString()) {
                    const uint str_len = *reinterpret_cast<const uint*>(p);
                    return sizeof(uint) + str_len;
                }
                else {
                    return prop->GetDictKeySize();
                }
            };

            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += get_key_len(key);

                    uint arr_size;
                    std::memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(uint);

                    AnyData::Array arr;
                    arr.reserve(arr_size);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint i = 0; i < arr_size; i++) {
                                uint str_size;
                                std::memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(uint);
                                auto str = string {reinterpret_cast<const char*>(data), str_size};
                                arr.emplace_back(std::move(str));
                                data += str_size;
                            }
                        }
                        else {
                            for (uint i = 0; i < arr_size; i++) {
                                if (prop->IsBaseTypeHash()) {
                                    arr.emplace_back(string {hash_resolver.ResolveHash(*reinterpret_cast<const hstring::hash_t*>(data + i * sizeof(hstring::hash_t)))});
                                }
                                else if (prop->IsBaseTypeEnum()) {
                                    int enum_value = 0;
                                    std::memcpy(&enum_value, data + static_cast<size_t>(i) * prop->GetBaseSize(), prop->GetBaseSize());
                                    arr.emplace_back(name_resolver.ResolveEnumValueName(prop->GetBaseTypeName(), enum_value));
                                }
                                else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->GetBaseSize()); \
    arr.push_back(static_cast<db_t>(*static_cast<const t*>(reinterpret_cast<const void*>(data + static_cast<size_t>(i) * prop->GetBaseSize()))))

                                    if (prop->IsInt8()) {
                                        PARSE_VALUE(int8, int);
                                    }
                                    else if (prop->IsInt16()) {
                                        PARSE_VALUE(int16, int);
                                    }
                                    else if (prop->IsInt32()) {
                                        PARSE_VALUE(int, int);
                                    }
                                    else if (prop->IsInt64()) {
                                        PARSE_VALUE(int64, int64);
                                    }
                                    else if (prop->IsUInt8()) {
                                        PARSE_VALUE(uint8, int);
                                    }
                                    else if (prop->IsUInt16()) {
                                        PARSE_VALUE(uint16, int);
                                    }
                                    else if (prop->IsUInt32()) {
                                        PARSE_VALUE(uint, int);
                                    }
                                    else if (prop->IsUInt64()) {
                                        PARSE_VALUE(uint64, int64);
                                    }
                                    else if (prop->IsSingleFloat()) {
                                        PARSE_VALUE(float, double);
                                    }
                                    else if (prop->IsDoubleFloat()) {
                                        PARSE_VALUE(double, double);
                                    }
                                    else if (prop->IsBool()) {
                                        PARSE_VALUE(bool, bool);
                                    }
                                    else {
                                        throw UnreachablePlaceException(LINE_STR);
                                    }

#undef PARSE_VALUE
                                }
                            }

                            data += static_cast<size_t>(arr_size) * prop->GetBaseSize();
                        }
                    }

                    string key_str = get_key_string(key);
                    dict.insert(std::make_pair(std::move(key_str), std::move(arr)));
                }
            }
            else if (prop->IsDictOfString()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += get_key_len(key);

                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    data += str_size;

                    string key_str = get_key_string(key);
                    dict.insert(std::make_pair(std::move(key_str), std::move(str)));
                }
            }
            else {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += get_key_len(key);

                    string key_str = get_key_string(key);

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = *reinterpret_cast<const hstring::hash_t*>(data);
                        dict.insert(std::make_pair(std::move(key_str), string {hash_resolver.ResolveHash(hash)}));
                        data += prop->GetBaseSize();
                    }
                    else if (prop->IsBaseTypeEnum()) {
                        int enum_value = 0;
                        std::memcpy(&enum_value, data, prop->GetBaseSize());
                        dict.insert(std::make_pair(std::move(key_str), name_resolver.ResolveEnumValueName(prop->GetBaseTypeName(), enum_value)));
                        data += prop->GetBaseSize();
                    }
                    else {
#define PARSE_VALUE(t, db_t) \
    RUNTIME_ASSERT(sizeof(t) == prop->GetBaseSize()); \
    dict.insert(std::make_pair(std::move(key_str), static_cast<db_t>(*static_cast<const t*>(reinterpret_cast<const void*>(data))))); \
    data += prop->GetBaseSize()

                        if (prop->IsInt8()) {
                            PARSE_VALUE(int8, int);
                        }
                        else if (prop->IsInt16()) {
                            PARSE_VALUE(int16, int);
                        }
                        else if (prop->IsInt32()) {
                            PARSE_VALUE(int, int);
                        }
                        else if (prop->IsInt64()) {
                            PARSE_VALUE(int64, int64);
                        }
                        else if (prop->IsUInt8()) {
                            PARSE_VALUE(uint8, int);
                        }
                        else if (prop->IsUInt16()) {
                            PARSE_VALUE(uint16, int);
                        }
                        else if (prop->IsUInt32()) {
                            PARSE_VALUE(uint, int);
                        }
                        else if (prop->IsUInt64()) {
                            PARSE_VALUE(uint64, int64);
                        }
                        else if (prop->IsSingleFloat()) {
                            PARSE_VALUE(float, double);
                        }
                        else if (prop->IsDoubleFloat()) {
                            PARSE_VALUE(double, double);
                        }
                        else if (prop->IsBool()) {
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

auto PropertiesSerializator::LoadPropertyFromValue(Properties* props, const Property* prop, const AnyData::Value& value, HashResolver& hash_resolver, NameResolver& name_resolver) -> bool
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(!prop->IsDisabled());
    RUNTIME_ASSERT(!prop->IsVirtual());
    RUNTIME_ASSERT(!prop->IsDisabled());

    // Implicit conversion to string
    string tmp_str;

    const auto can_read_to_string = [](const auto& some_value) -> bool {
        return some_value.index() == AnyData::STRING_VALUE || some_value.index() == AnyData::INT_VALUE || //
            some_value.index() == AnyData::INT64_VALUE || some_value.index() == AnyData::DOUBLE_VALUE || some_value.index() == AnyData::BOOL_VALUE;
    };

    const auto read_to_string = [&tmp_str](const auto& some_value) -> const string& {
        switch (some_value.index()) {
        case AnyData::STRING_VALUE:
            return std::get<string>(some_value);
        case AnyData::INT_VALUE:
            return tmp_str = _str("{}", std::get<int>(some_value));
        case AnyData::INT64_VALUE:
            return tmp_str = _str("{}", std::get<int64>(some_value));
        case AnyData::DOUBLE_VALUE:
            return tmp_str = _str("{}", std::get<double>(some_value));
        case AnyData::BOOL_VALUE:
            return tmp_str = _str("{}", std::get<bool>(some_value));
        default:
            throw UnreachablePlaceException(LINE_STR);
        }
    };

    // Implicit conversion to number
    const auto can_convert_str_to_number = [](const auto& some_value) -> bool {
        // Todo: check if converted value fits to target bounds
        return _str(std::get<string>(some_value)).isNumber();
    };

    const auto convert_str_to_number = [prop](const auto& some_value, uint8* pod_data) {
        if (prop->IsInt8()) {
            *static_cast<int8*>(reinterpret_cast<void*>(pod_data)) = static_cast<int8>(_str(std::get<string>(some_value)).toInt());
        }
        else if (prop->IsInt16()) {
            *static_cast<int16*>(reinterpret_cast<void*>(pod_data)) = static_cast<int16>(_str(std::get<string>(some_value)).toInt());
        }
        else if (prop->IsInt32()) {
            *static_cast<int*>(reinterpret_cast<void*>(pod_data)) = static_cast<int>(_str(std::get<string>(some_value)).toInt());
        }
        else if (prop->IsInt64()) {
            *static_cast<int64*>(reinterpret_cast<void*>(pod_data)) = static_cast<int64>(_str(std::get<string>(some_value)).toInt64());
        }
        else if (prop->IsUInt8()) {
            *static_cast<uint8*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint8>(_str(std::get<string>(some_value)).toUInt());
        }
        else if (prop->IsUInt16()) {
            *static_cast<int16*>(reinterpret_cast<void*>(pod_data)) = static_cast<int16>(_str(std::get<string>(some_value)).toUInt());
        }
        else if (prop->IsUInt32()) {
            *static_cast<uint*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint>(_str(std::get<string>(some_value)).toUInt());
        }
        else if (prop->IsUInt64()) {
            *static_cast<uint64*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint64>(_str(std::get<string>(some_value)).toUInt64());
        }
        else if (prop->IsSingleFloat()) {
            *static_cast<float*>(reinterpret_cast<void*>(pod_data)) = static_cast<float>(_str(std::get<string>(some_value)).toFloat());
        }
        else if (prop->IsDoubleFloat()) {
            *static_cast<double*>(reinterpret_cast<void*>(pod_data)) = static_cast<double>(_str(std::get<string>(some_value)).toDouble());
        }
        else if (prop->IsBool()) {
            *static_cast<bool*>(reinterpret_cast<void*>(pod_data)) = static_cast<bool>(_str(std::get<string>(some_value)).toBool());
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    };

    // Parse value
    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            if (value.index() != AnyData::STRING_VALUE) {
                WriteLog("Wrong hash value type, property {}", prop->GetName());
                return false;
            }

            const auto h = hash_resolver.ToHashedString(std::get<string>(value)).as_hash();
            props->SetRawData(prop, reinterpret_cast<const uint8*>(&h), prop->GetBaseSize());
        }
        else if (prop->IsBaseTypeEnum()) {
            int enum_value;

            if (value.index() == AnyData::STRING_VALUE) {
                auto is_error = false;
                enum_value = name_resolver.ResolveEnumValue(prop->GetBaseTypeName(), std::get<string>(value), &is_error);
                if (is_error) {
                    return false;
                }
            }
            else if (value.index() == AnyData::INT_VALUE || value.index() == AnyData::INT64_VALUE) {
                // Todo: validate integer value to fit in enum range
                enum_value = static_cast<int>(value.index() == AnyData::INT_VALUE ? std::get<int>(value) : std::get<int64>(value));
            }
            else {
                WriteLog("Wrong enum value type, property {}", prop->GetName());
                return false;
            }

            props->SetRawData(prop, reinterpret_cast<const uint8*>(&enum_value), prop->GetBaseSize());
        }
        else if (prop->IsInt() || prop->IsFloat() || prop->IsBool()) {
            if (value.index() == AnyData::ARRAY_VALUE || value.index() == AnyData::DICT_VALUE) {
                WriteLog("Wrong integer value type (array or dict), property {}", prop->GetName());
                return false;
            }

            if (value.index() == AnyData::STRING_VALUE && !can_convert_str_to_number(value)) {
                WriteLog("Wrong numeric string '{}', property {}", std::get<string>(value), prop->GetName());
                return false;
            }

#define PARSE_VALUE(t) \
    do { \
        if (prop->IsInt8()) { \
            *static_cast<int8*>(reinterpret_cast<void*>(pod_data)) = static_cast<int8>(std::get<t>(value)); \
        } \
        else if (prop->IsInt16()) { \
            *static_cast<int16*>(reinterpret_cast<void*>(pod_data)) = static_cast<int16>(std::get<t>(value)); \
        } \
        else if (prop->IsInt32()) { \
            *static_cast<int*>(reinterpret_cast<void*>(pod_data)) = static_cast<int>(std::get<t>(value)); \
        } \
        else if (prop->IsInt64()) { \
            *static_cast<int64*>(reinterpret_cast<void*>(pod_data)) = static_cast<int64>(std::get<t>(value)); \
        } \
        else if (prop->IsUInt8()) { \
            *static_cast<uint8*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint8>(std::get<t>(value)); \
        } \
        else if (prop->IsUInt16()) { \
            *static_cast<int16*>(reinterpret_cast<void*>(pod_data)) = static_cast<int16>(std::get<t>(value)); \
        } \
        else if (prop->IsUInt32()) { \
            *static_cast<uint*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint>(std::get<t>(value)); \
        } \
        else if (prop->IsUInt64()) { \
            *static_cast<uint64*>(reinterpret_cast<void*>(pod_data)) = static_cast<uint64>(std::get<t>(value)); \
        } \
        else if (prop->IsSingleFloat()) { \
            *static_cast<float*>(reinterpret_cast<void*>(pod_data)) = static_cast<float>(std::get<t>(value)); \
        } \
        else if (prop->IsDoubleFloat()) { \
            *static_cast<double*>(reinterpret_cast<void*>(pod_data)) = static_cast<double>(std::get<t>(value)); \
        } \
        else if (prop->IsBool()) { \
            *static_cast<bool*>(reinterpret_cast<void*>(pod_data)) = (std::get<t>(value) != static_cast<t>(0)); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

            uint8 pod_data[8];
            if (value.index() == AnyData::INT_VALUE) {
                PARSE_VALUE(int);
            }
            else if (value.index() == AnyData::INT64_VALUE) {
                PARSE_VALUE(int64);
            }
            else if (value.index() == AnyData::DOUBLE_VALUE) {
                PARSE_VALUE(double);
            }
            else if (value.index() == AnyData::BOOL_VALUE) {
                PARSE_VALUE(bool);
            }
            else if (value.index() == AnyData::STRING_VALUE) {
                convert_str_to_number(value, pod_data);
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }

#undef PARSE_VALUE

            props->SetRawData(prop, pod_data, prop->GetBaseSize());
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }
    else if (prop->IsString()) {
        if (!can_read_to_string(value)) {
            WriteLog("Wrong string value type, property {}", prop->GetName());
            return false;
        }

        const auto& str = read_to_string(value);

        props->SetRawData(prop, reinterpret_cast<const uint8*>(str.c_str()), static_cast<uint>(str.length()));
    }
    else if (prop->IsArray()) {
        if (value.index() != AnyData::ARRAY_VALUE) {
            WriteLog("Wrong array value type, property {}", prop->GetName());
            return false;
        }

        const auto& arr = std::get<AnyData::Array>(value);

        if (arr.empty()) {
            props->SetRawData(prop, nullptr, 0);
            return true;
        }

        if (prop->IsBaseTypeHash()) {
            if (arr[0].index() != AnyData::STRING_VALUE) {
                WriteLog("Wrong array hash element value type, property {}", prop->GetName());
                return false;
            }

            uint data_size = static_cast<uint>(arr.size()) * sizeof(hstring::hash_t);
            auto data = unique_ptr<uint8>(new uint8[data_size]);

            for (size_t i = 0; i < arr.size(); i++) {
                RUNTIME_ASSERT(arr[i].index() == AnyData::STRING_VALUE);

                const auto h = hash_resolver.ToHashedString(std::get<string>(arr[i])).as_hash();
                *reinterpret_cast<hstring::hash_t*>(data.get() + i * sizeof(hstring::hash_t)) = h;
            }

            props->SetRawData(prop, data.get(), data_size);
        }
        else if (prop->IsBaseTypeEnum()) {
            if (arr[0].index() != AnyData::STRING_VALUE && arr[0].index() != AnyData::INT_VALUE && arr[0].index() != AnyData::INT64_VALUE) {
                WriteLog("Wrong array enum element value type, property {}", prop->GetName());
                return false;
            }

            uint data_size = static_cast<uint>(arr.size()) * prop->GetBaseSize();
            auto data = unique_ptr<uint8>(new uint8[data_size]);

            for (size_t i = 0; i < arr.size(); i++) {
                RUNTIME_ASSERT(arr[i].index() == arr[0].index());

                int enum_value;

                if (arr[i].index() == AnyData::STRING_VALUE) {
                    auto is_error = false;
                    enum_value = name_resolver.ResolveEnumValue(prop->GetBaseTypeName(), std::get<string>(arr[i]), &is_error);
                    if (is_error) {
                        return false;
                    }
                }
                else {
                    enum_value = static_cast<int>(arr[i].index() == AnyData::INT_VALUE ? std::get<int>(arr[i]) : std::get<int64>(arr[i]));
                }

                std::memcpy(data.get() + i * prop->GetBaseSize(), &enum_value, prop->GetBaseSize());
            }

            props->SetRawData(prop, data.get(), data_size);
        }
        else if (prop->IsInt() || prop->IsFloat() || prop->IsBool()) {
            if (arr[0].index() == AnyData::ARRAY_VALUE || arr[0].index() == AnyData::DICT_VALUE) {
                WriteLog("Wrong array element value type (array or dict), property {}", prop->GetName());
                return false;
            }

            if (arr[0].index() == AnyData::STRING_VALUE) {
                for (size_t i = 0; i < arr.size(); i++) {
                    RUNTIME_ASSERT(arr[i].index() == AnyData::STRING_VALUE);
                    if (!can_convert_str_to_number(arr[i])) {
                        WriteLog("Wrong array numeric string '{}' at index {}, property {}", std::get<string>(value), i, prop->GetName());
                        return false;
                    }
                }
            }

            uint data_size = prop->GetBaseSize() * static_cast<uint>(arr.size());
            auto data = unique_ptr<uint8>(new uint8[data_size]);
            const auto arr_element_index = arr[0].index();

#define PARSE_VALUE(t) \
    do { \
        for (size_t i = 0; i < arr.size(); i++) { \
            RUNTIME_ASSERT(arr[i].index() == arr_element_index); \
            if (arr_element_index == AnyData::INT_VALUE) { \
                *static_cast<t*>(reinterpret_cast<void*>(data.get() + i * prop->GetBaseSize())) = static_cast<t>(std::get<int>(arr[i])); \
            } \
            else if (arr_element_index == AnyData::INT64_VALUE) { \
                *static_cast<t*>(reinterpret_cast<void*>(data.get() + i * prop->GetBaseSize())) = static_cast<t>(std::get<int64>(arr[i])); \
            } \
            else if (arr_element_index == AnyData::DOUBLE_VALUE) { \
                *static_cast<t*>(reinterpret_cast<void*>(data.get() + i * prop->GetBaseSize())) = static_cast<t>(std::get<double>(arr[i])); \
            } \
            else if (arr_element_index == AnyData::BOOL_VALUE) { \
                *static_cast<t*>(reinterpret_cast<void*>(data.get() + i * prop->GetBaseSize())) = static_cast<t>(std::get<bool>(arr[i])); \
            } \
            else if (arr_element_index == AnyData::STRING_VALUE) { \
                convert_str_to_number(arr[i], data.get() + i * prop->GetBaseSize()); \
            } \
            else { \
                throw UnreachablePlaceException(LINE_STR); \
            } \
        } \
    } while (false)

            if (prop->IsInt8()) {
                PARSE_VALUE(int8);
            }
            else if (prop->IsInt16()) {
                PARSE_VALUE(int16);
            }
            else if (prop->IsInt32()) {
                PARSE_VALUE(int);
            }
            else if (prop->IsInt64()) {
                PARSE_VALUE(int64);
            }
            else if (prop->IsUInt8()) {
                PARSE_VALUE(uint8);
            }
            else if (prop->IsUInt16()) {
                PARSE_VALUE(uint16);
            }
            else if (prop->IsUInt32()) {
                PARSE_VALUE(uint);
            }
            else if (prop->IsUInt64()) {
                PARSE_VALUE(uint64);
            }
            else if (prop->IsSingleFloat()) {
                PARSE_VALUE(float);
            }
            else if (prop->IsDoubleFloat()) {
                PARSE_VALUE(double);
            }
            else if (prop->IsBool()) {
                PARSE_VALUE(bool);
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }

#undef PARSE_VALUE

            props->SetRawData(prop, data.get(), data_size);
        }
        else {
            RUNTIME_ASSERT(prop->IsArrayOfString());

            if (!can_read_to_string(arr[0])) {
                WriteLog("Wrong array element value type, property {}", prop->GetName());
                return false;
            }

            uint data_size = sizeof(uint);
            for (size_t i = 0; i < arr.size(); i++) {
                RUNTIME_ASSERT(can_read_to_string(arr[i]));

                string_view str = read_to_string(arr[i]);
                data_size += sizeof(uint) + static_cast<uint>(str.length());
            }

            auto data = unique_ptr<uint8>(new uint8[data_size]);
            *reinterpret_cast<uint*>(data.get()) = static_cast<uint>(arr.size());

            size_t data_pos = sizeof(uint);
            for (size_t i = 0; i < arr.size(); i++) {
                const auto& str = read_to_string(arr[i]);
                *reinterpret_cast<uint*>(data.get() + data_pos) = static_cast<uint>(str.length());
                if (!str.empty()) {
                    std::memcpy(data.get() + data_pos + sizeof(uint), str.c_str(), str.length());
                }
                data_pos += sizeof(uint) + str.length();
            }
            RUNTIME_ASSERT(data_pos == data_size);

            props->SetRawData(prop, data.get(), data_size);
        }
    }
    else if (prop->IsDict()) {
        if (value.index() != AnyData::DICT_VALUE) {
            WriteLog("Wrong dict value type, property {}", prop->GetName());
            return false;
        }

        const auto& dict = std::get<AnyData::Dict>(value);

        if (dict.empty()) {
            props->SetRawData(prop, nullptr, 0);
            return true;
        }

        // Measure data length
        uint data_size = 0;
        bool wrong_input = false;
        for (const auto& [dict_key, dict_value] : dict) {
            if (prop->IsDictKeyString()) {
                data_size += sizeof(uint) + static_cast<uint>(dict_key.length());
            }
            else {
                data_size += prop->GetDictKeySize();
            }

            if (prop->IsDictOfArray()) {
                if (dict_value.index() != AnyData::ARRAY_VALUE) {
                    WriteLog("Wrong dict array value type, property {}", prop->GetName());
                    wrong_input = true;
                    break;
                }

                const auto& arr = std::get<AnyData::Array>(dict_value);

                data_size += sizeof(uint);

                if (prop->IsBaseTypeHash()) {
                    for (const auto& e : arr) {
                        if (e.index() != AnyData::STRING_VALUE) {
                            WriteLog("Wrong dict array element hash value type, property {}", prop->GetName());
                            wrong_input = true;
                            break;
                        }
                    }

                    data_size += static_cast<uint>(arr.size()) * sizeof(hstring::hash_t);
                }
                else if (prop->IsBaseTypeEnum()) {
                    for (const auto& e : arr) {
                        if (e.index() != AnyData::STRING_VALUE) {
                            WriteLog("Wrong dict array element enum value type, property {}", prop->GetName());
                            wrong_input = true;
                            break;
                        }
                    }

                    data_size += static_cast<uint>(arr.size()) * prop->GetBaseSize();
                }
                else if (prop->IsDictOfArrayOfString()) {
                    for (const auto& e : arr) {
                        if (!can_read_to_string(e)) {
                            WriteLog("Wrong dict array element string value type, property {}", prop->GetName());
                            wrong_input = true;
                            break;
                        }

                        data_size += sizeof(uint) + static_cast<uint>(read_to_string(e).length());
                    }
                }
                else {
                    for (const auto& e : arr) {
                        if (e.index() == AnyData::ARRAY_VALUE || e.index() == AnyData::DICT_VALUE || e.index() != arr[0].index()) {
                            WriteLog("Wrong dict array element value type, property {}", prop->GetName());
                            wrong_input = true;
                            break;
                        }

                        if (e.index() == AnyData::STRING_VALUE && !can_convert_str_to_number(e)) {
                            WriteLog("Wrong dict array string number element value '{}', property {}", std::get<string>(e), prop->GetName());
                            wrong_input = true;
                            break;
                        }
                    }

                    data_size += static_cast<int>(arr.size()) * prop->GetBaseSize();
                }
            }
            else if (prop->IsDictOfString()) {
                if (!can_read_to_string(dict_value)) {
                    WriteLog("Wrong dict string element value type, property {}", prop->GetName());
                    wrong_input = true;
                    break;
                }

                data_size += static_cast<uint>(read_to_string(dict_value).length());
            }
            else if (prop->IsBaseTypeHash()) {
                if (dict_value.index() != AnyData::STRING_VALUE) {
                    WriteLog("Wrong dict hash element value type, property {}", prop->GetName());
                    wrong_input = true;
                    break;
                }

                data_size += sizeof(hstring::hash_t);
            }
            else if (prop->IsBaseTypeEnum()) {
                if (dict_value.index() != AnyData::STRING_VALUE && dict_value.index() != AnyData::INT_VALUE && dict_value.index() != AnyData::INT64_VALUE) {
                    WriteLog("Wrong dict enum element value type, property {}", prop->GetName());
                    wrong_input = true;
                    break;
                }

                data_size += prop->GetBaseSize();
            }
            else {
                if (dict_value.index() == AnyData::ARRAY_VALUE || dict_value.index() == AnyData::DICT_VALUE) {
                    WriteLog("Wrong dict number element value type (array or dict), property {}", prop->GetName());
                    wrong_input = true;
                    break;
                }

                if (dict_value.index() == AnyData::STRING_VALUE && !can_convert_str_to_number(dict_value)) {
                    WriteLog("Wrong dict string number element value '{}', property {}", std::get<string>(dict_value), prop->GetName());
                    wrong_input = true;
                    break;
                }

                data_size += prop->GetBaseSize();
            }

            if (wrong_input) {
                break;
            }
        }

        if (wrong_input) {
            return false;
        }

        // Write data
        auto data = unique_ptr<uint8>(new uint8[data_size]);
        size_t data_pos = 0;

        for (const auto& [dict_key, dict_value] : dict) {
            // Key
            if (prop->IsDictKeyString()) {
                const uint key_len = static_cast<uint>(dict_key.length());
                std::memcpy(data.get() + data_pos, &key_len, sizeof(key_len));
                data_pos += sizeof(key_len);
                std::memcpy(data.get() + data_pos, dict_key.c_str(), dict_key.length());
                data_pos += dict_key.length();
            }
            else if (prop->IsDictKeyHash()) {
                *reinterpret_cast<hstring::hash_t*>(data.get() + data_pos) = hash_resolver.ToHashedString(dict_key).as_hash();
                data_pos += prop->GetDictKeySize();
            }
            else if (prop->IsDictKeyEnum()) {
                auto is_error = false;
                int enum_value = name_resolver.ResolveEnumValue(prop->GetDictKeyTypeName(), dict_key, &is_error);
                std::memcpy(data.get() + data_pos, &enum_value, prop->GetBaseSize());
                data_pos += prop->GetDictKeySize();
                if (is_error) {
                    return false;
                }
            }
            else if (prop->GetDictKeySize() == 1) {
                *reinterpret_cast<int8*>(data.get() + data_pos) = static_cast<int8>(_str(dict_key).toInt64());
                data_pos += prop->GetDictKeySize();
            }
            else if (prop->GetDictKeySize() == 2) {
                *reinterpret_cast<int16*>(data.get() + data_pos) = static_cast<int16>(_str(dict_key).toInt64());
                data_pos += prop->GetDictKeySize();
            }
            else if (prop->GetDictKeySize() == 4) {
                *reinterpret_cast<int*>(data.get() + data_pos) = static_cast<int>(_str(dict_key).toInt64());
                data_pos += prop->GetDictKeySize();
            }
            else if (prop->GetDictKeySize() == 8) {
                *reinterpret_cast<int64*>(data.get() + data_pos) = _str(dict_key).toInt64();
                data_pos += prop->GetDictKeySize();
            }
            else {
                throw UnreachablePlaceException(LINE_STR);
            }

            // Value
            if (prop->IsDictOfArray()) {
                const auto& arr = std::get<AnyData::Array>(dict_value);

                *reinterpret_cast<uint*>(data.get() + data_pos) = static_cast<uint>(arr.size());
                data_pos += sizeof(uint);

                if (prop->IsBaseTypeEnum()) {
                    for (const auto& e : arr) {
                        int enum_value;

                        if (e.index() == AnyData::STRING_VALUE) {
                            auto is_error = false;
                            enum_value = name_resolver.ResolveEnumValue(prop->GetBaseTypeName(), std::get<string>(e), &is_error);
                            if (is_error) {
                                return false;
                            }
                        }
                        else {
                            enum_value = static_cast<int>(e.index() == AnyData::INT_VALUE ? std::get<int>(e) : std::get<int64>(e));
                        }

                        std::memcpy(data.get() + data_pos, &enum_value, prop->GetBaseSize());
                        data_pos += prop->GetBaseSize();
                    }
                }
                else if (prop->IsBaseTypeHash()) {
                    for (const auto& e : arr) {
                        *reinterpret_cast<hstring::hash_t*>(data.get() + data_pos) = hash_resolver.ToHashedString(std::get<string>(e)).as_hash();
                        data_pos += sizeof(hstring::hash_t);
                    }
                }
                else if (prop->IsDictOfArrayOfString()) {
                    for (const auto& e : arr) {
                        const auto& str = read_to_string(e);
                        *reinterpret_cast<uint*>(data.get() + data_pos) = static_cast<uint>(str.length());
                        data_pos += sizeof(uint);
                        if (!str.empty()) {
                            std::memcpy(data.get() + data_pos, str.c_str(), str.length());
                            data_pos += static_cast<uint>(str.length());
                        }
                    }
                }
                else {
                    for (const auto& e : arr) {
#define PARSE_VALUE(t) \
    do { \
        RUNTIME_ASSERT(sizeof(t) == prop->GetBaseSize()); \
        if (e.index() == AnyData::INT_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<int>(e)); \
        } \
        else if (e.index() == AnyData::INT64_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<int64>(e)); \
        } \
        else if (e.index() == AnyData::DOUBLE_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<double>(e)); \
        } \
        else if (e.index() == AnyData::BOOL_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<bool>(e)); \
        } \
        else if (e.index() == AnyData::STRING_VALUE) { \
            convert_str_to_number(e, data.get() + data_pos); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                        if (prop->IsInt8()) {
                            PARSE_VALUE(int8);
                        }
                        else if (prop->IsInt16()) {
                            PARSE_VALUE(int16);
                        }
                        else if (prop->IsInt32()) {
                            PARSE_VALUE(int);
                        }
                        else if (prop->IsInt64()) {
                            PARSE_VALUE(int64);
                        }
                        else if (prop->IsUInt8()) {
                            PARSE_VALUE(uint8);
                        }
                        else if (prop->IsUInt16()) {
                            PARSE_VALUE(uint16);
                        }
                        else if (prop->IsUInt32()) {
                            PARSE_VALUE(uint);
                        }
                        else if (prop->IsUInt64()) {
                            PARSE_VALUE(uint64);
                        }
                        else if (prop->IsSingleFloat()) {
                            PARSE_VALUE(float);
                        }
                        else if (prop->IsDoubleFloat()) {
                            PARSE_VALUE(double);
                        }
                        else if (prop->IsBool()) {
                            PARSE_VALUE(bool);
                        }
                        else {
                            throw UnreachablePlaceException(LINE_STR);
                        }

#undef PARSE_VALUE

                        data_pos += prop->GetBaseSize();
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                const auto& str = read_to_string(dict_value);

                *reinterpret_cast<uint*>(data.get() + data_pos) = static_cast<uint>(str.length());
                data_pos += sizeof(uint);

                if (!str.empty()) {
                    std::memcpy(data.get() + data_pos, str.c_str(), str.length());
                    data_pos += str.length();
                }
            }
            else {
                if (prop->IsBaseTypeHash()) {
                    *reinterpret_cast<hstring::hash_t*>(data.get() + data_pos) = hash_resolver.ToHashedString(std::get<string>(dict_value)).as_hash();
                }
                else if (prop->IsBaseTypeEnum()) {
                    int enum_value;

                    if (dict_value.index() == AnyData::STRING_VALUE) {
                        auto is_error = false;
                        enum_value = name_resolver.ResolveEnumValue(prop->GetBaseTypeName(), std::get<string>(dict_value), &is_error);
                        if (is_error) {
                            return false;
                        }
                    }
                    else {
                        enum_value = static_cast<int>(dict_value.index() == AnyData::INT_VALUE ? std::get<int>(dict_value) : std::get<int64>(dict_value));
                    }

                    std::memcpy(data.get() + data_pos, &enum_value, prop->GetBaseSize());
                }
                else {
#define PARSE_VALUE(t) \
    do { \
        RUNTIME_ASSERT(sizeof(t) == prop->GetBaseSize()); \
        if (dict_value.index() == AnyData::INT_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<int>(dict_value)); \
        } \
        else if (dict_value.index() == AnyData::INT64_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<int64>(dict_value)); \
        } \
        else if (dict_value.index() == AnyData::DOUBLE_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<double>(dict_value)); \
        } \
        else if (dict_value.index() == AnyData::BOOL_VALUE) { \
            *reinterpret_cast<t*>(data.get() + data_pos) = static_cast<t>(std::get<bool>(dict_value)); \
        } \
        else if (dict_value.index() == AnyData::STRING_VALUE) { \
            convert_str_to_number(dict_value, data.get() + data_pos); \
        } \
        else { \
            throw UnreachablePlaceException(LINE_STR); \
        } \
    } while (false)

                    if (prop->IsInt8()) {
                        PARSE_VALUE(int8);
                    }
                    else if (prop->IsInt16()) {
                        PARSE_VALUE(int16);
                    }
                    else if (prop->IsInt32()) {
                        PARSE_VALUE(int);
                    }
                    else if (prop->IsInt64()) {
                        PARSE_VALUE(int64);
                    }
                    else if (prop->IsUInt8()) {
                        PARSE_VALUE(uint8);
                    }
                    else if (prop->IsUInt16()) {
                        PARSE_VALUE(uint16);
                    }
                    else if (prop->IsUInt32()) {
                        PARSE_VALUE(uint);
                    }
                    else if (prop->IsUInt64()) {
                        PARSE_VALUE(uint64);
                    }
                    else if (prop->IsSingleFloat()) {
                        PARSE_VALUE(float);
                    }
                    else if (prop->IsDoubleFloat()) {
                        PARSE_VALUE(double);
                    }
                    else if (prop->IsBool()) {
                        PARSE_VALUE(bool);
                    }
                    else {
                        throw UnreachablePlaceException(LINE_STR);
                    }

#undef PARSE_VALUE
                }

                data_pos += prop->GetBaseSize();
            }
        }

        RUNTIME_ASSERT(data_pos == data_size);

        props->SetRawData(prop, data.get(), data_size);
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    return true;
}
