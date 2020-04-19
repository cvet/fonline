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

#include "Properties.h"
#include "Log.h"
#include "ScriptSystem.h"
#include "StringUtils.h"

unique_ptr<void, std::function<void(void*)>> Property::CreateRefValue(uchar* data, uint data_size)
{
    /*if (dataType == Property::Array)
    {
        if (isArrayOfString)
        {
            if (data_size)
            {
                uint arr_size;
                memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(uint);
                CScriptArray* arr = CScriptArray::Create(asObjType, arr_size);
                RUNTIME_ASSERT(arr);
                for (uint i = 0; i < arr_size; i++)
                {
                    uint str_size;
                    memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str((char*)data, str_size);
                    arr->SetValue(i, &str);
                    data += str_size;
                }
                return arr;
            }
            else
            {
                CScriptArray* arr = CScriptArray::Create(asObjType);
                RUNTIME_ASSERT(arr);
                return arr;
            }
        }
        else
        {
            uint element_size = engine->GetSizeOfPrimitiveType(asObjType->GetSubTypeId());
            uint arr_size = data_size / element_size;
            CScriptArray* arr = CScriptArray::Create(asObjType, arr_size);
            RUNTIME_ASSERT(arr);
            if (arr_size)
                memcpy(arr->At(0), data, arr_size * element_size);
            return arr;
        }
    }
    else if (dataType == Property::Dict)
    {
        CScriptDict* dict = CScriptDict::Create(asObjType);
        RUNTIME_ASSERT(dict);
        if (data_size)
        {
            uint key_element_size = engine->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(0));
            if (isDictOfArray)
            {
                asITypeInfo* arr_type = asObjType->GetSubType(1);
                uint arr_element_size = engine->GetSizeOfPrimitiveType(arr_type->GetSubTypeId());
                uchar* data_end = data + data_size;
                while (data < data_end)
                {
                    void* key = data;
                    data += key_element_size;
                    uint arr_size;
                    memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(uint);
                    CScriptArray* arr = CScriptArray::Create(arr_type, arr_size);
                    RUNTIME_ASSERT(arr);
                    if (arr_size)
                    {
                        if (isDictOfArrayOfString)
                        {
                            for (uint i = 0; i < arr_size; i++)
                            {
                                uint str_size;
                                memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(uint);
                                string str((char*)data, str_size);
                                arr->SetValue(i, &str);
                                data += str_size;
                            }
                        }
                        else
                        {
                            memcpy(arr->At(0), data, arr_size * arr_element_size);
                            data += arr_size * arr_element_size;
                        }
                    }
                    dict->Set(key, &arr);
                    arr->Release();
                }
            }
            else if (isDictOfString)
            {
                uchar* data_end = data + data_size;
                while (data < data_end)
                {
                    void* key = data;
                    data += key_element_size;
                    uint str_size;
                    memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);
                    string str((char*)data, str_size);
                    dict->Set(key, &str);
                    data += str_size;
                }
            }
            else
            {
                uint value_element_size = engine->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(1));
                uint whole_element_size = key_element_size + value_element_size;
                uint dict_size = data_size / whole_element_size;
                for (uint i = 0; i < dict_size; i++)
                    dict->Set(data + i * whole_element_size, data + i * whole_element_size + key_element_size);
            }
        }
        return dict;
    }
    else
    {
        RUNTIME_ASSERT(!"Unexpected type");
    }*/
    return nullptr;
}

uchar* Property::ExpandComplexValueData(void* value, uint& data_size, bool& need_delete)
{
    /*need_delete = false;
    if (dataType == Property::String)
    {
        string& str = *(string*)value;
        data_size = (uint)str.length();
        return data_size ? (uchar*)str.c_str() : nullptr;
    }
    else if (dataType == Property::Array)
    {
        CScriptArray* arr = (CScriptArray*)value;
        if (isArrayOfString)
        {
            data_size = 0;
            uint arr_size = arr->GetSize();
            if (arr_size)
            {
                need_delete = true;

                // Calculate size
                data_size += sizeof(uint);
                for (uint i = 0; i < arr_size; i++)
                {
                    string& str = *(string*)arr->At(i);
                    data_size += sizeof(uint) + (uint)str.length();
                }

                // Make buffer
                uchar* init_buf = new uchar[data_size];
                uchar* buf = init_buf;
                memcpy(buf, &arr_size, sizeof(uint));
                buf += sizeof(uint);
                for (uint i = 0; i < arr_size; i++)
                {
                    string& str = *(string*)arr->At(i);
                    uint str_size = (uint)str.length();
                    memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (str_size)
                    {
                        memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
                return init_buf;
            }
            return nullptr;
        }
        else
        {
            int element_size;
            int type_id = arr->GetElementTypeId();
            if (type_id & asTYPEID_MASK_OBJECT)
                element_size = sizeof(asPWORD);
            else
                element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType(type_id);

            data_size = (arr ? arr->GetSize() * element_size : 0);
            return data_size ? (uchar*)arr->At(0) : nullptr;
        }
    }
    else if (dataType == Property::Dict)
    {
        CScriptDict* dict = (CScriptDict*)value;
        if (isDictOfArray)
        {
            data_size = (dict ? dict->GetSize() : 0);
            if (data_size)
            {
                need_delete = true;
                uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(0));
                uint arr_element_size =
                    asObjType->GetEngine()->GetSizeOfPrimitiveType(asObjType->GetSubType(1)->GetSubTypeId());

                // Calculate size
                data_size = 0;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);
                for (const auto& kv : dict_map)
                {
                    CScriptArray* arr = *(CScriptArray**)kv.second;
                    uint arr_size = (arr ? arr->GetSize() : 0);
                    data_size += key_element_size + sizeof(uint);
                    if (isDictOfArrayOfString)
                    {
                        for (uint i = 0; i < arr_size; i++)
                        {
                            string& str = *(string*)arr->At(i);
                            data_size += sizeof(uint) + (uint)str.length();
                        }
                    }
                    else
                    {
                        data_size += arr_size * arr_element_size;
                    }
                }

                // Make buffer
                uchar* init_buf = new uchar[data_size];
                uchar* buf = init_buf;
                for (const auto& kv : dict_map)
                {
                    CScriptArray* arr = *(CScriptArray**)kv.second;
                    memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    uint arr_size = (arr ? arr->GetSize() : 0);
                    memcpy(buf, &arr_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (arr_size)
                    {
                        if (isDictOfArrayOfString)
                        {
                            for (uint i = 0; i < arr_size; i++)
                            {
                                string& str = *(string*)arr->At(i);
                                uint str_size = (uint)str.length();
                                memcpy(buf, &str_size, sizeof(uint));
                                buf += sizeof(uint);
                                if (str_size)
                                {
                                    memcpy(buf, str.c_str(), str_size);
                                    buf += arr_size;
                                }
                            }
                        }
                        else
                        {
                            memcpy(buf, arr->At(0), arr_size * arr_element_size);
                            buf += arr_size * arr_element_size;
                        }
                    }
                }
                return init_buf;
            }
        }
        else if (isDictOfString)
        {
            uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(0));
            data_size = (dict ? dict->GetSize() : 0);
            if (data_size)
            {
                need_delete = true;

                // Calculate size
                data_size = 0;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);
                for (const auto& kv : dict_map)
                {
                    string& str = *(string*)kv.second;
                    uint str_size = (uint)str.length();
                    data_size += key_element_size + sizeof(uint) + str_size;
                }

                // Make buffer
                uchar* init_buf = new uchar[data_size];
                uchar* buf = init_buf;
                for (const auto& kv : dict_map)
                {
                    string& str = *(string*)kv.second;
                    memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    uint str_size = (uint)str.length();
                    memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (str_size)
                    {
                        memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
                return init_buf;
            }
        }
        else
        {
            uint key_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(0));
            uint value_element_size = asObjType->GetEngine()->GetSizeOfPrimitiveType(asObjType->GetSubTypeId(1));
            uint whole_element_size = key_element_size + value_element_size;
            data_size = (dict ? dict->GetSize() * whole_element_size : 0);
            if (data_size)
            {
                need_delete = true;
                uchar* init_buf = new uchar[data_size];
                uchar* buf = init_buf;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);
                for (const auto& kv : dict_map)
                {
                    memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    memcpy(buf, kv.second, value_element_size);
                    buf += value_element_size;
                }
                return init_buf;
            }
        }
        return nullptr;
    }
    else
    {
        RUNTIME_ASSERT(!"Unexpected type");
    }*/
    return nullptr;
}

string Property::GetName()
{
    return propName;
}

string Property::GetTypeName()
{
    return typeName;
}

uint Property::GetRegIndex()
{
    return regIndex;
}

int Property::GetEnumValue()
{
    return enumValue;
}

Property::AccessType Property::GetAccess()
{
    return accessType;
}

uint Property::GetBaseSize()
{
    return baseSize;
}

bool Property::IsPOD()
{
    return dataType == Property::POD;
}

bool Property::IsDict()
{
    return dataType == Property::Dict;
}

bool Property::IsHash()
{
    return isHash;
}

bool Property::IsResource()
{
    return isResource;
}

bool Property::IsEnum()
{
    return isEnumDataType;
}

bool Property::IsReadable()
{
    return isReadable;
}

bool Property::IsWritable()
{
    return isWritable;
}

bool Property::IsConst()
{
    return isConst;
}

bool Property::IsTemporary()
{
    return isTemporary;
}

bool Property::IsNoHistory()
{
    return isNoHistory;
}

Properties::Properties(PropertyRegistrator* reg)
{
    registrator = reg;
    RUNTIME_ASSERT(registrator);

    sendIgnoreEntity = nullptr;
    sendIgnoreProperty = nullptr;

    // Allocate POD data
    if (!registrator->podDataPool.empty())
    {
        podData = registrator->podDataPool.back();
        registrator->podDataPool.pop_back();
    }
    else
    {
        podData = new uchar[registrator->wholePodDataSize];
    }
    memzero(podData, registrator->wholePodDataSize);

    // Complex data
    complexData.resize(registrator->complexPropertiesCount);
    complexDataSizes.resize(registrator->complexPropertiesCount);
}

Properties::Properties(const Properties& other) : Properties(other.registrator)
{
    // Copy POD data
    memcpy(&podData[0], &other.podData[0], registrator->wholePodDataSize);

    // Copy complex data
    for (size_t i = 0; i < other.complexData.size(); i++)
    {
        uint size = other.complexDataSizes[i];
        complexDataSizes[i] = size;
        complexData[i] = (size ? new uchar[size] : NULL);
        if (size)
            memcpy(complexData[i], other.complexData[i], size);
    }
}

Properties::~Properties()
{
    registrator->podDataPool.push_back(podData);

    for (size_t i = 0; i < complexData.size(); i++)
        SAFEDELA(complexData[i]);
}

Properties& Properties::operator=(const Properties& other)
{
    RUNTIME_ASSERT(registrator == other.registrator);

    // Copy POD data
    memcpy(&podData[0], &other.podData[0], registrator->wholePodDataSize);

    // Copy complex data
    for (size_t i = 0; i < registrator->registeredProperties.size(); i++)
    {
        Property* prop = registrator->registeredProperties[i];
        if (prop->complexDataIndex != uint(-1))
            SetRawData(prop, other.complexData[prop->complexDataIndex], other.complexDataSizes[prop->complexDataIndex]);
    }

    return *this;
}

Property* Properties::FindByEnum(int enum_value)
{
    return registrator->FindByEnum(enum_value);
}

void* Properties::FindData(const string& property_name)
{
    Property* prop = registrator->Find(property_name);
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(prop->podDataOffset != uint(-1));
    return prop ? &podData[prop->podDataOffset] : nullptr;
}

uint Properties::StoreData(bool with_protected, PUCharVec** all_data, UIntVec** all_data_sizes)
{
    uint whole_size = 0;
    *all_data = &storeData;
    *all_data_sizes = &storeDataSizes;
    storeData.resize(0);
    storeDataSizes.resize(0);

    // Store POD properties data
    storeData.push_back(podData);
    storeDataSizes.push_back((uint)registrator->publicPodDataSpace.size() +
        (with_protected ? (uint)registrator->protectedPodDataSpace.size() : 0));
    whole_size += storeDataSizes.back();

    // Calculate complex data to send
    storeDataComplexIndicies =
        (with_protected ? registrator->publicProtectedComplexDataProps : registrator->publicComplexDataProps);
    for (size_t i = 0; i < storeDataComplexIndicies.size();)
    {
        Property* prop = registrator->registeredProperties[storeDataComplexIndicies[i]];
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
        if (!complexDataSizes[prop->complexDataIndex])
            storeDataComplexIndicies.erase(storeDataComplexIndicies.begin() + i);
        else
            i++;
    }

    // Store complex properties data
    if (!storeDataComplexIndicies.empty())
    {
        storeData.push_back((uchar*)&storeDataComplexIndicies[0]);
        storeDataSizes.push_back((uint)storeDataComplexIndicies.size() * sizeof(short));
        whole_size += storeDataSizes.back();
        for (size_t i = 0; i < storeDataComplexIndicies.size(); i++)
        {
            Property* prop = registrator->registeredProperties[storeDataComplexIndicies[i]];
            storeData.push_back(complexData[prop->complexDataIndex]);
            storeDataSizes.push_back(complexDataSizes[prop->complexDataIndex]);
            whole_size += storeDataSizes.back();
        }
    }
    return whole_size;
}

void Properties::RestoreData(PUCharVec& all_data, UIntVec& all_data_sizes)
{
    // Restore POD data
    uint publicSize = (uint)registrator->publicPodDataSpace.size();
    uint protectedSize = (uint)registrator->protectedPodDataSpace.size();
    RUNTIME_ASSERT((all_data_sizes[0] == publicSize || all_data_sizes[0] == publicSize + protectedSize));
    if (all_data_sizes[0])
        memcpy(podData, all_data[0], all_data_sizes[0]);

    // Restore complex data
    if (all_data.size() > 1)
    {
        uint comlplex_data_count = all_data_sizes[1] / sizeof(ushort);
        RUNTIME_ASSERT(comlplex_data_count > 0);
        UShortVec complex_indicies(comlplex_data_count);
        memcpy(&complex_indicies[0], all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++)
        {
            RUNTIME_ASSERT(complex_indicies[i] < registrator->registeredProperties.size());
            Property* prop = registrator->registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
            uint data_size = all_data_sizes[2 + i];
            uchar* data = all_data[2 + i];
            SetRawData(prop, data, data_size);
        }
    }
}

void Properties::RestoreData(UCharVecVec& all_data)
{
    PUCharVec all_data_ext(all_data.size());
    UIntVec all_data_sizes(all_data.size());
    for (size_t i = 0; i < all_data.size(); i++)
    {
        all_data_ext[i] = (!all_data[i].empty() ? &all_data[i][0] : nullptr);
        all_data_sizes[i] = (uint)all_data[i].size();
    }
    RestoreData(all_data_ext, all_data_sizes);
}

static const char* ReadToken(const char* str, string& result)
{
    if (!*str)
        return nullptr;

    const char* begin;
    const char* s = str;

    uint length;
    utf8::Decode(s, &length);
    while (length == 1 && (*s == ' ' || *s == '\t'))
        utf8::Decode(++s, &length);

    if (!*s)
        return nullptr;

    if (length == 1 && *s == '{')
    {
        s++;
        begin = s;

        int braces = 1;
        while (*s)
        {
            if (length == 1 && *s == '\\')
            {
                ++s;
                if (*s)
                {
                    utf8::Decode(s, &length);
                    s += length;
                }
            }
            else if (length == 1 && *s == '{')
            {
                braces++;
                s++;
            }
            else if (length == 1 && *s == '}')
            {
                braces--;
                if (braces == 0)
                    break;
                s++;
            }
            else
            {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }
    else
    {
        begin = s;
        while (*s)
        {
            if (length == 1 && *s == '\\')
            {
                utf8::Decode(++s, &length);
                s += length;
            }
            else if (length == 1 && (*s == ' ' || *s == '\t'))
            {
                break;
            }
            else
            {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }

    result.assign(begin, (uint)(s - begin));
    return *s ? s + 1 : s;
}

static string CodeString(const string& str, int deep)
{
    bool need_braces = false;
    if (deep > 0 && (str.empty() || str.find_first_of(" \t") != string::npos))
        need_braces = true;
    if (!need_braces && deep == 0 && !str.empty() &&
        (str.find_first_of(" \t") == 0 || str.find_last_of(" \t") == str.length() - 1))
        need_braces = true;
    if (!need_braces && str.length() >= 2 && str.back() == '\\' && str[str.length() - 2] == ' ')
        need_braces = true;

    string result;
    result.reserve(str.length() * 2);

    if (need_braces)
        result.append("{");

    const char* s = str.c_str();
    uint length;
    while (*s)
    {
        utf8::Decode(s, &length);
        if (length == 1)
        {
            switch (*s)
            {
            case '\r':
                break;
            case '\n':
                result.append("\\n");
                break;
            case '{':
                result.append("\\{");
                break;
            case '}':
                result.append("\\}");
                break;
            case '\\':
                result.append("\\\\");
                break;
            default:
                result.append(s, 1);
                break;
            }
        }
        else
        {
            result.append(s, length);
        }

        s += length;
    }

    if (need_braces)
        result.append("}");

    return result;
}

static string DecodeString(const string& str)
{
    if (str.empty())
        return str;

    string result;
    result.reserve(str.length());

    const char* s = str.c_str();
    uint length;

    utf8::Decode(s, &length);
    bool is_braces = (length == 1 && *s == '{');
    if (is_braces)
        s++;

    while (*s)
    {
        utf8::Decode(s, &length);
        if (length == 1 && *s == '\\')
        {
            utf8::Decode(++s, &length);

            switch (*s)
            {
            case 'n':
                result.append("\n");
                break;
            case '{':
                result.append("{");
                break;
            case '}':
                result.append("\n");
                break;
            case '\\':
                result.append("\\");
                break;
            default:
                result.append(1, '\\');
                result.append(s, length);
                break;
            }
        }
        else
        {
            result.append(s, length);
        }

        s += length;
    }

    if (is_braces && length == 1 && result.back() == '}')
        result.pop_back();

    return result;
}

/*string WriteValue(void* ptr, int type_id, asITypeInfo* as_obj_type, bool* is_hashes, int deep)
{
    if (!(type_id & asTYPEID_MASK_OBJECT))
    {
        RUNTIME_ASSERT(type_id != asTYPEID_VOID);

#define VALUE_AS(ctype) (*(ctype*)(ptr))
#define CHECK_PRIMITIVE(astype, ctype) \
    if (type_id == astype) \
    return _str("{}", VALUE_AS(ctype))
#define CHECK_PRIMITIVE_EXT(astype, ctype, ctype_str) \
    if (type_id == astype) \
    return _str("{}", (ctype_str)VALUE_AS(ctype))

        if (is_hashes[deep])
        {
            RUNTIME_ASSERT(type_id == asTYPEID_UINT32);
            return VALUE_AS(hash) ? CodeString(_str().parseHash(VALUE_AS(hash)), deep) : CodeString("", deep);
        }

        CHECK_PRIMITIVE(asTYPEID_BOOL, bool);
        CHECK_PRIMITIVE_EXT(asTYPEID_INT8, char, int);
        CHECK_PRIMITIVE(asTYPEID_INT16, short);
        CHECK_PRIMITIVE(asTYPEID_INT32, int);
        CHECK_PRIMITIVE(asTYPEID_INT64, int64);
        CHECK_PRIMITIVE(asTYPEID_UINT8, uchar);
        CHECK_PRIMITIVE(asTYPEID_UINT16, ushort);
        CHECK_PRIMITIVE(asTYPEID_UINT32, uint);
        CHECK_PRIMITIVE(asTYPEID_UINT64, uint64);
        CHECK_PRIMITIVE(asTYPEID_DOUBLE, double);
        CHECK_PRIMITIVE(asTYPEID_FLOAT, float);
        // return Script::GetEnumValueName(Script::GetEngine()->GetTypeDeclaration(type_id), VALUE_AS(int));
        return "";

#undef VALUE_AS
#undef CHECK_PRIMITIVE
#undef CHECK_PRIMITIVE_EXT
    }
    else if (Str::Compare(as_obj_type->GetName(), "string"))
    {
        string& str = *(string*)ptr;
        return CodeString(str, deep);
    }
    else if (Str::Compare(as_obj_type->GetName(), "array"))
    {
        string result = (deep > 0 ? "{" : "");
        CScriptArray* arr = (CScriptArray*)ptr;
        if (deep > 0)
            arr = *(CScriptArray**)ptr;
        asUINT arr_size = arr->GetSize();
        if (arr_size > 0)
        {
            int value_type_id = as_obj_type->GetSubTypeId(0);
            asITypeInfo* value_type = as_obj_type->GetSubType(0);
            for (asUINT i = 0; i < arr_size; i++)
                result.append(WriteValue(arr->At(i), value_type_id, value_type, is_hashes, deep + 1)).append(" ");
            result.pop_back();
        }
        if (deep > 0)
            result.append("}");
        return result;
    }
    else if (Str::Compare(as_obj_type->GetName(), "dict"))
    {
        string result = (deep > 0 ? "{" : "");
        CScriptDict* dict = (CScriptDict*)ptr;
        if (dict->GetSize() > 0)
        {
            int key_type_id = as_obj_type->GetSubTypeId(0);
            int value_type_id = as_obj_type->GetSubTypeId(1);
            asITypeInfo* key_type = as_obj_type->GetSubType(0);
            asITypeInfo* value_type = as_obj_type->GetSubType(1);
            vector<pair<void*, void*>> dict_map;
            dict->GetMap(dict_map);
            for (const auto& dict_kv : dict_map)
            {
                result.append(WriteValue(dict_kv.first, key_type_id, key_type, is_hashes, deep + 1)).append(" ");
                result.append(WriteValue(dict_kv.second, value_type_id, value_type, is_hashes, deep + 2)).append(" ");
            }
            result.pop_back();
        }
        if (deep > 0)
            result.append("}");
        return result;
    }
    throw UnreachablePlaceException(LINE_STR);
    return "";
}

void* ReadValue(
    const char* value, int type_id, asITypeInfo* as_obj_type, bool* is_hashes, int deep, void* pod_buf, bool& is_error)
{
    RUNTIME_ASSERT(deep <= 3);

    if (!(type_id & asTYPEID_MASK_OBJECT))
    {
        RUNTIME_ASSERT(type_id != asTYPEID_VOID);

#define CHECK_PRIMITIVE(astype, ctype, ato) \
    if (type_id == astype) \
    { \
        ctype v = (ctype)_str(value).ato(); \
        memcpy(pod_buf, &v, sizeof(ctype)); \
        return pod_buf; \
    }

        if (is_hashes[deep])
        {
            RUNTIME_ASSERT(type_id == asTYPEID_UINT32);
            hash v = _str(DecodeString(value)).toHash();
            memcpy(pod_buf, &v, sizeof(v));
            return pod_buf;
        }

        CHECK_PRIMITIVE(asTYPEID_BOOL, bool, toBool);
        CHECK_PRIMITIVE(asTYPEID_INT8, char, toInt);
        CHECK_PRIMITIVE(asTYPEID_INT16, short, toInt);
        CHECK_PRIMITIVE(asTYPEID_INT32, int, toInt);
        CHECK_PRIMITIVE(asTYPEID_INT64, int64, toInt64);
        CHECK_PRIMITIVE(asTYPEID_UINT8, uchar, toUInt);
        CHECK_PRIMITIVE(asTYPEID_UINT16, ushort, toUInt);
        CHECK_PRIMITIVE(asTYPEID_UINT32, uint, toUInt);
        CHECK_PRIMITIVE(asTYPEID_UINT64, uint64, toUInt64);
        CHECK_PRIMITIVE(asTYPEID_DOUBLE, double, toDouble);
        CHECK_PRIMITIVE(asTYPEID_FLOAT, float, toFloat);

        // int v = Script::GetEnumValue(Script::GetEngine()->GetTypeDeclaration(type_id), value, is_error);
        // memcpy(pod_buf, &v, sizeof(v));
        // return pod_buf;
        return 0;

#undef CHECK_PRIMITIVE
    }
    else if (Str::Compare(as_obj_type->GetName(), "string"))
    {
        // string* str = (string*)Script::GetEngine()->CreateScriptObject(as_obj_type);
        // *str = DecodeString(value);
        // return str;
        return 0;
    }
    else if (Str::Compare(as_obj_type->GetName(), "array"))
    {
        CScriptArray* arr = CScriptArray::Create(as_obj_type);
        int value_type_id = as_obj_type->GetSubTypeId(0);
        asITypeInfo* value_type = as_obj_type->GetSubType(0);
        string str;
        uchar arr_pod_buf[8];
        while ((value = ReadToken(value, str)))
        {
            void* v = ReadValue(str.c_str(), value_type_id, value_type, is_hashes, deep + 1, arr_pod_buf, is_error);
            arr->InsertLast(value_type_id & asTYPEID_OBJHANDLE ? &v : v);
            // if (v != arr_pod_buf)
            //    Script::GetEngine()->ReleaseScriptObject(v, value_type);
        }
        return arr;
    }
    else if (Str::Compare(as_obj_type->GetName(), "dict"))
    {
        CScriptDict* dict = CScriptDict::Create(as_obj_type);
        int key_type_id = as_obj_type->GetSubTypeId(0);
        int value_type_id = as_obj_type->GetSubTypeId(1);
        asITypeInfo* key_type = as_obj_type->GetSubType(0);
        asITypeInfo* value_type = as_obj_type->GetSubType(1);
        string str1, str2;
        uchar dict_pod_buf1[8];
        uchar dict_pod_buf2[8];
        while ((value = ReadToken(value, str1)) && (value = ReadToken(value, str2)))
        {
            void* v1 = ReadValue(str1.c_str(), key_type_id, key_type, is_hashes, deep + 1, dict_pod_buf1, is_error);
            void* v2 = ReadValue(str2.c_str(), value_type_id, value_type, is_hashes, deep + 2, dict_pod_buf2, is_error);
            dict->Set(key_type_id & asTYPEID_OBJHANDLE ? &v1 : v1, value_type_id & asTYPEID_OBJHANDLE ? &v2 : v2);
            // if (v1 != dict_pod_buf1)
            //    Script::GetEngine()->ReleaseScriptObject(v1, key_type);
            // if (v2 != dict_pod_buf2)
            //    Script::GetEngine()->ReleaseScriptObject(v2, value_type);
        }
        return dict;
    }
    throw UnreachablePlaceException(LINE_STR);
    return nullptr;
}*/

bool Properties::LoadFromText(const StrMap& key_values)
{
    /*bool is_error = false;
    for (const auto& kv : key_values)
    {
        const string& key = kv.first;
        const string& value = kv.second;

        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_')
            continue;

        // Find property
        Property* prop = registrator->Find(key);
        if (!prop || (prop->podDataOffset == uint(-1) && prop->complexDataIndex == uint(-1)))
        {
            if (!prop)
                WriteLog("Unknown property '{}'.\n", key);
            else
                WriteLog("Invalid property '{}' for reading.\n", prop->GetName());

            is_error = true;
            continue;
        }

        // Parse
        if (!LoadPropertyFromText(prop, value))
            is_error = true;
    }
    return !is_error;*/
    return false;
}

void Properties::SaveToText(StrMap& key_values, Properties* base)
{
    RUNTIME_ASSERT((!base || registrator == base->registrator));

    for (auto& prop : registrator->registeredProperties)
    {
        // Skip pure virtual properties
        if (prop->podDataOffset == uint(-1) && prop->complexDataIndex == uint(-1))
            continue;

        // Skip temporary properties
        if (prop->isTemporary)
            continue;

        // Skip same
        if (base)
        {
            if (prop->podDataOffset != uint(-1))
            {
                if (!memcmp(&podData[prop->podDataOffset], &base->podData[prop->podDataOffset], prop->baseSize))
                    continue;
            }
            else
            {
                if (!complexDataSizes[prop->complexDataIndex] && !base->complexDataSizes[prop->complexDataIndex])
                    continue;
                if (complexDataSizes[prop->complexDataIndex] == base->complexDataSizes[prop->complexDataIndex] &&
                    !memcmp(complexData[prop->complexDataIndex], base->complexData[prop->complexDataIndex],
                        complexDataSizes[prop->complexDataIndex]))
                    continue;
            }
        }
        else
        {
            if (prop->podDataOffset != uint(-1))
            {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(prop->baseSize <= sizeof(pod_zero));
                if (!memcmp(&podData[prop->podDataOffset], &pod_zero, prop->baseSize))
                    continue;
            }
            else
            {
                if (!complexDataSizes[prop->complexDataIndex])
                    continue;
            }
        }

        // Serialize to text and store in map
        key_values.insert(std::make_pair(prop->propName, SavePropertyToText(prop)));
    }
}

bool Properties::LoadPropertyFromText(Property* prop, const string& text)
{
    /*RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(registrator == prop->registrator);
    RUNTIME_ASSERT((prop->podDataOffset != uint(-1) || prop->complexDataIndex != uint(-1)));
    bool is_error = false;

    // Parse
    uchar pod_buf[8];
    bool is_hashes[] = {
        prop->isHash || prop->isResource, prop->isHashSubType0, prop->isHashSubType1, prop->isHashSubType2};
    void* value = ReadValue(text.c_str(), prop->asObjTypeId, prop->asObjType, is_hashes, 0, pod_buf, is_error);

    // Assign
    if (prop->podDataOffset != uint(-1))
    {
        RUNTIME_ASSERT(value == pod_buf);
        prop->SetPropRawData(this, pod_buf, prop->baseSize);
    }
    else if (prop->complexDataIndex != uint(-1))
    {
        bool need_delete;
        uint data_size;
        uchar* data = prop->ExpandComplexValueData(value, data_size, need_delete);
        prop->SetPropRawData(this, data, data_size);
        if (need_delete)
            SAFEDELA(data);
        // Script::GetEngine()->ReleaseScriptObject(value, prop->asObjType);
    }

    return !is_error;*/
    return false;
}

string Properties::SavePropertyToText(Property* prop)
{
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(registrator == prop->registrator);
    RUNTIME_ASSERT((prop->podDataOffset != uint(-1) || prop->complexDataIndex != uint(-1)));

    uint data_size;
    void* data = GetRawData(prop, data_size);

    /*void* value = data;
    string str;
    if (prop->dataType == Property::String)
    {
        value = &str;
        if (data_size)
            str.assign((char*)data, data_size);
    }
    else if (prop->dataType == Property::Array || prop->dataType == Property::Dict)
    {
        value = prop->CreateRefValue((uchar*)data, data_size);
    }

    bool is_hashes[] = {
        prop->isHash || prop->isResource, prop->isHashSubType0, prop->isHashSubType1, prop->isHashSubType2};
    string text = WriteValue(value, prop->asObjTypeId, prop->asObjType, is_hashes, 0);

    if (prop->dataType == Property::Array || prop->dataType == Property::Dict)
        prop->ReleaseRefValue(value);*/

    // return text;
    return "";
}

void Properties::SetSendIgnore(Property* prop, Entity* entity)
{
    if (prop)
    {
        RUNTIME_ASSERT(sendIgnoreEntity == nullptr);
        RUNTIME_ASSERT(sendIgnoreProperty == nullptr);
    }
    else
    {
        RUNTIME_ASSERT(sendIgnoreEntity != nullptr);
        RUNTIME_ASSERT(sendIgnoreProperty != nullptr);
    }

    sendIgnoreEntity = entity;
    sendIgnoreProperty = prop;
}

uint Properties::GetRawDataSize(Property* prop)
{
    uint data_size = 0;
    GetRawData(prop, data_size);
    return data_size;
}

uchar* Properties::GetRawData(Property* prop, uint& data_size)
{
    if (prop->dataType == Property::POD)
    {
        RUNTIME_ASSERT(prop->podDataOffset != uint(-1));
        data_size = prop->baseSize;
        return &podData[prop->podDataOffset];
    }

    RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
    data_size = complexDataSizes[prop->complexDataIndex];
    return complexData[prop->complexDataIndex];
}

void Properties::SetRawData(Property* prop, uchar* data, uint data_size)
{
    if (prop->IsPOD())
    {
        RUNTIME_ASSERT(prop->podDataOffset != uint(-1));
        RUNTIME_ASSERT(prop->baseSize == data_size);
        memcpy(&podData[prop->podDataOffset], data, data_size);
    }
    else
    {
        RUNTIME_ASSERT(prop->complexDataIndex != uint(-1));
        if (data_size != complexDataSizes[prop->complexDataIndex])
        {
            complexDataSizes[prop->complexDataIndex] = data_size;
            SAFEDELA(complexData[prop->complexDataIndex]);
            if (data_size)
                complexData[prop->complexDataIndex] = new uchar[data_size];
        }
        if (data_size)
            memcpy(complexData[prop->complexDataIndex], data, data_size);
    }
}

void Properties::SetValueFromData(Property* prop, uchar* data, uint data_size)
{
    /*if (dataType == Property::String)
    {
        string str;
        if (data_size)
            str.assign((char*)data, data_size);
        GenericSet(entity, &str);
    }
    else if (dataType == Property::POD)
    {
        RUNTIME_ASSERT(data_size == baseSize);
        GenericSet(entity, data);
    }
    else if (dataType == Property::Array || dataType == Property::Dict)
    {
        auto value = CreateRefValue(data, data_size);
        GenericSet(entity, value.get());
    }
    else
    {
        RUNTIME_ASSERT(!"Unexpected type");
    }*/
}

int Properties::GetPODValueAsInt(Property* prop)
{
    RUNTIME_ASSERT(prop->dataType == Property::POD);

    if (prop->isBoolDataType)
    {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    else if (prop->isFloatDataType)
    {
        if (prop->baseSize == 4)
            return (int)GetValue<float>(prop);
        else if (prop->baseSize == 8)
            return (int)GetValue<double>(prop);
    }
    else if (prop->isSignedIntDataType)
    {
        if (prop->baseSize == 1)
            return (int)GetValue<char>(prop);
        if (prop->baseSize == 2)
            return (int)GetValue<short>(prop);
        if (prop->baseSize == 4)
            return (int)GetValue<int>(prop);
        if (prop->baseSize == 8)
            return (int)GetValue<int64>(prop);
    }
    else
    {
        if (prop->baseSize == 1)
            return (int)GetValue<uchar>(prop);
        if (prop->baseSize == 2)
            return (int)GetValue<ushort>(prop);
        if (prop->baseSize == 4)
            return (int)GetValue<uint>(prop);
        if (prop->baseSize == 8)
            return (int)GetValue<uint64>(prop);
    }

    throw UnreachablePlaceException(LINE_STR);
}

void Properties::SetPODValueAsInt(Property* prop, int value)
{
    RUNTIME_ASSERT(prop->dataType == Property::POD);

    if (prop->isBoolDataType)
    {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->isFloatDataType)
    {
        if (prop->baseSize == 4)
            SetValue<float>(prop, (float)value);
        else if (prop->baseSize == 8)
            SetValue<double>(prop, (double)value);
    }
    else if (prop->isSignedIntDataType)
    {
        if (prop->baseSize == 1)
            SetValue<char>(prop, (char)value);
        else if (prop->baseSize == 2)
            SetValue<short>(prop, (short)value);
        else if (prop->baseSize == 4)
            SetValue<int>(prop, (int)value);
        else if (prop->baseSize == 8)
            SetValue<int64>(prop, (int64)value);
    }
    else
    {
        if (prop->baseSize == 1)
            SetValue<uchar>(prop, (uchar)value);
        else if (prop->baseSize == 2)
            SetValue<ushort>(prop, (ushort)value);
        else if (prop->baseSize == 4)
            SetValue<uint>(prop, (uint)value);
        else if (prop->baseSize == 8)
            SetValue<uint64>(prop, (uint64)value);
    }
}

int Properties::GetValueAsInt(int enum_value)
{
    Property* prop = registrator->FindByEnum(enum_value);
    if (!prop)
        throw PropertiesException("Enum not found", enum_value);
    if (!prop->IsPOD())
        throw PropertiesException("Can't retreive integer value from non POD property", prop->GetName());
    if (!prop->IsReadable())
        throw PropertiesException("Can't retreive integer value from non readable property", prop->GetName());

    return GetPODValueAsInt(prop);
}

void Properties::SetValueAsInt(int enum_value, int value)
{
    Property* prop = registrator->FindByEnum(enum_value);
    if (!prop)
        throw PropertiesException("Enum not found", enum_value);
    if (!prop->IsPOD())
        throw PropertiesException("Can't set integer value to non POD property", prop->GetName());
    if (!prop->IsWritable())
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());

    SetPODValueAsInt(prop, value);
}

void Properties::SetValueAsIntByName(const string& enum_name, int value)
{
    Property* prop = registrator->Find(enum_name);
    if (!prop)
        throw PropertiesException("Enum not found", enum_name);
    if (!prop->IsPOD())
        throw PropertiesException("Can't set by name integer value from non POD property", prop->GetName());
    if (!prop->IsWritable())
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());

    SetPODValueAsInt(prop, value);
}

void Properties::SetValueAsIntProps(int enum_value, int value)
{
    Property* prop = registrator->FindByEnum(enum_value);
    if (!prop)
        throw PropertiesException("Enum not found", enum_value);
    if (!prop->IsPOD())
        throw PropertiesException("Can't set integer value to non POD property", prop->GetName());
    if (!prop->IsWritable())
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    if (prop->accessType & Property::VirtualMask)
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());

    if (prop->isBoolDataType)
    {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->isFloatDataType)
    {
        if (prop->baseSize == 4)
            SetValue<float>(prop, (float)value);
        else if (prop->baseSize == 8)
            SetValue<double>(prop, (double)value);
    }
    else if (prop->isSignedIntDataType)
    {
        if (prop->baseSize == 1)
            SetValue<char>(prop, (char)value);
        else if (prop->baseSize == 2)
            SetValue<short>(prop, (short)value);
        else if (prop->baseSize == 4)
            SetValue<int>(prop, (int)value);
        else if (prop->baseSize == 8)
            SetValue<int64>(prop, (int64)value);
    }
    else
    {
        if (prop->baseSize == 1)
            SetValue<uchar>(prop, (uchar)value);
        else if (prop->baseSize == 2)
            SetValue<ushort>(prop, (ushort)value);
        else if (prop->baseSize == 4)
            SetValue<uint>(prop, (uint)value);
        else if (prop->baseSize == 8)
            SetValue<uint64>(prop, (uint64)value);
    }
}

PropertyRegistrator::PropertyRegistrator(bool is_server)
{
    isServer = is_server;
}

PropertyRegistrator::~PropertyRegistrator()
{
    for (size_t i = 0; i < registeredProperties.size(); i++)
        delete registeredProperties[i];
    for (size_t i = 0; i < podDataPool.size(); i++)
        delete[] podDataPool[i];
}

Property* PropertyRegistrator::Register(Property::AccessType access, const type_info& type, const string& name)
{
#define ISTYPE(t) (type.hash_code() == typeid(t).hash_code())

    Property::DataType data_type = Property::DataType::POD;
    uint data_size = 0;
    bool is_int_data_type = false;
    bool is_signed_int_data_type = false;
    bool is_float_data_type = false;
    bool is_bool_data_type = false;
    bool is_enum_data_type = false;
    bool is_array_of_string = false;
    bool is_dict_of_string = false;
    bool is_dict_of_array = false;
    bool is_dict_of_array_of_string = false;
    bool is_hash_sub0 = false;
    bool is_hash_sub1 = false;
    bool is_hash_sub2 = false;

    if (ISTYPE(int) || ISTYPE(uint) || ISTYPE(char) || ISTYPE(uchar) || ISTYPE(short) || ISTYPE(ushort) ||
        ISTYPE(int64) || ISTYPE(uint64) || ISTYPE(float) || ISTYPE(double) || ISTYPE(bool))
    {
        data_type = Property::POD;

        if (ISTYPE(char) || ISTYPE(uchar) || ISTYPE(bool))
            data_size = 1;
        else if (ISTYPE(short) || ISTYPE(ushort))
            data_size = 2;
        else if (ISTYPE(int) || ISTYPE(uint) || ISTYPE(float))
            data_size = 4;
        else if (ISTYPE(int64) || ISTYPE(uint64) || ISTYPE(double))
            data_size = 8;
        else
            throw UnreachablePlaceException(LINE_STR);

        is_int_data_type = (ISTYPE(int) || ISTYPE(uint) || ISTYPE(char) || ISTYPE(uchar) || ISTYPE(short) ||
            ISTYPE(ushort) || ISTYPE(int64) || ISTYPE(uint64));
        is_signed_int_data_type = (ISTYPE(int) || ISTYPE(char) || ISTYPE(short) || ISTYPE(int64));
        is_float_data_type = (ISTYPE(float) || ISTYPE(double));
        is_bool_data_type = ISTYPE(bool);
        // is_enum_data_type = (type_id > asTYPEID_DOUBLE);
    }
    else if (ISTYPE(string))
    {
        data_type = Property::String;
        data_size = sizeof(string);
    }
    else if (ISTYPE(vector<int>))
    {
        data_type = Property::Array;
        data_size = sizeof(void*);

        bool is_array_of_pod = (ISTYPE(vector<int>));
        is_array_of_string = ISTYPE(vector<string>);
        is_hash_sub0 = ISTYPE(vector<hash>);
    }
    else if (ISTYPE(vector<int>))
    {
        data_type = Property::Dict;
        data_size = sizeof(void*);

        /*int value_sub_type_id = as_obj_type->GetSubTypeId(1);
        asITypeInfo* value_sub_type = as_obj_type->GetSubType(1);
        if (value_sub_type_id & asTYPEID_MASK_OBJECT)
        {
            is_dict_of_string = Str::Compare(value_sub_type->GetName(), "string");
            is_dict_of_array = Str::Compare(value_sub_type->GetName(), "array");
            is_dict_of_array_of_string = (is_dict_of_array && value_sub_type->GetSubType() &&
                Str::Compare(value_sub_type->GetSubType()->GetName(), "string"));
            bool is_dict_array_of_pod = !(value_sub_type->GetSubTypeId() & asTYPEID_MASK_OBJECT);
            if (!is_dict_of_string && !is_dict_array_of_pod && !is_dict_of_array_of_string)
            {
                WriteLog(
                    "Invalid property type '{}', dict value must have POD/string type or array of POD/string type.\n",
                    type_name);
                return nullptr;
            }
        }
        if (_str(type_name).str().find("resource") != string::npos)
        {
            WriteLog("Invalid property type '{}', dict elements can't be resource type.\n", type_name);
            return nullptr;
        }

        string t = _str(type_name).str();
        is_hash_sub0 = (t.find("hash") != string::npos && t.find(",", t.find("hash")) != string::npos);
        is_hash_sub1 =
            (t.find("hash") != string::npos && t.find(",", t.find("hash")) == string::npos && !is_dict_of_array);
        is_hash_sub2 =
            (t.find("hash") != string::npos && t.find(",", t.find("hash")) == string::npos && is_dict_of_array);*/
    }
    else
    {
        // WriteLog("Invalid property type '{}'.\n", type_name);
        // return nullptr;
    }

    // Allocate property
    Property* prop = new Property();
    uint reg_index = (uint)registeredProperties.size();

    // Disallow set or get accessors
    bool disable_get = false;
    bool disable_set = false;
    if (isServer && (access & Property::ClientOnlyMask))
        disable_get = disable_set = true;
    if (!isServer && (access & Property::ServerOnlyMask))
        disable_get = disable_set = true;
    if (!isServer && ((access & Property::PublicMask) || (access & Property::ProtectedMask)) &&
        !(access & Property::ModifiableMask))
        disable_set = true;
    // if (is_const)
    //    disable_set = true;

    disable_get = false;

    // Register default getter
    if (disable_get)
        return nullptr;

    // Register setter
    if (!disable_set)
    {
    }

    // POD property data offset
    uint data_base_offset = uint(-1);
    if (data_type == Property::POD && !disable_get && !(access & Property::VirtualMask))
    {
        bool is_public = (access & Property::PublicMask) != 0;
        bool is_protected = (access & Property::ProtectedMask) != 0;
        BoolVec& space =
            (is_public ? publicPodDataSpace : (is_protected ? protectedPodDataSpace : privatePodDataSpace));

        uint space_size = (uint)space.size();
        uint space_pos = 0;
        while (space_pos + data_size <= space_size)
        {
            bool fail = false;
            for (uint i = 0; i < data_size; i++)
            {
                if (space[space_pos + i])
                {
                    fail = true;
                    break;
                }
            }
            if (!fail)
                break;
            space_pos += data_size;
        }

        uint new_size = space_pos + data_size;
        new_size += 8 - new_size % 8;
        if (new_size > (uint)space.size())
            space.resize(new_size);

        for (uint i = 0; i < data_size; i++)
            space[space_pos + i] = true;

        data_base_offset = space_pos;

        wholePodDataSize =
            (uint)(publicPodDataSpace.size() + protectedPodDataSpace.size() + privatePodDataSpace.size());
        // RUNTIME_ASSERT((wholePodDataSize % 8) == 0);
    }

    // Complex property data index
    uint complex_data_index = uint(-1);
    if (data_type != Property::POD && (!disable_get || !disable_set) && !(access & Property::VirtualMask))
    {
        complex_data_index = complexPropertiesCount++;
        if (access & Property::PublicMask)
        {
            publicComplexDataProps.push_back((ushort)reg_index);
            publicProtectedComplexDataProps.push_back((ushort)reg_index);
        }
        else if (access & Property::ProtectedMask)
        {
            protectedComplexDataProps.push_back((ushort)reg_index);
            publicProtectedComplexDataProps.push_back((ushort)reg_index);
        }
        else if (access & Property::PrivateMask)
        {
            privateComplexDataProps.push_back((ushort)reg_index);
        }
        else
        {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    // Make entry
    prop->registrator = this;
    prop->regIndex = reg_index;
    prop->getIndex = (!disable_get ? getPropertiesCount++ : uint(-1));
    // prop->enumValue = enum_value;
    prop->complexDataIndex = complex_data_index;
    prop->podDataOffset = data_base_offset;
    prop->baseSize = data_size;

    prop->propName = _str(name).replace('.', '_');
    // prop->typeName = type_name;
    // prop->componentName = component_name;
    prop->dataType = data_type;
    prop->accessType = access;
    // prop->isConst = is_const;
    // prop->asObjTypeId = type_id;
    // prop->asObjType = as_obj_type;
    // prop->isHash = type_name == "hash";
    prop->isHashSubType0 = is_hash_sub0;
    prop->isHashSubType1 = is_hash_sub1;
    prop->isHashSubType2 = is_hash_sub2;
    // prop->isResource = type_name == "resource";
    prop->isIntDataType = is_int_data_type;
    prop->isSignedIntDataType = is_signed_int_data_type;
    prop->isFloatDataType = is_float_data_type;
    prop->isEnumDataType = is_enum_data_type;
    prop->isBoolDataType = is_bool_data_type;
    prop->isArrayOfString = is_array_of_string;
    prop->isDictOfString = is_dict_of_string;
    prop->isDictOfArray = is_dict_of_array;
    prop->isDictOfArrayOfString = is_dict_of_array_of_string;
    prop->isReadable = !disable_get;
    prop->isWritable = !disable_set;
    // prop->isTemporary = (defaultTemporary || is_temporary);
    // prop->isNoHistory = (defaultNoHistory || is_no_history);
    // prop->checkMinValue = (min_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->checkMaxValue = (max_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->minValue = (min_value ? *min_value : 0);
    // prop->maxValue = (max_value ? *max_value : 0);

    registeredProperties.push_back(prop);

    // Fix POD data offsets
    for (size_t i = 0, j = registeredProperties.size(); i < j; i++)
    {
        Property* prop = registeredProperties[i];
        if (prop->podDataOffset == uint(-1))
            continue;

        if (prop->accessType & Property::ProtectedMask)
            prop->podDataOffset += (uint)publicPodDataSpace.size();
        else if (prop->accessType & Property::PrivateMask)
            prop->podDataOffset += (uint)publicPodDataSpace.size() + (uint)protectedPodDataSpace.size();
    }

    return prop;
}

void PropertyRegistrator::RegisterComponent(const string& name)
{
    hash name_hash = _str(name).toHash();
    RUNTIME_ASSERT(!registeredComponents.count(name_hash));
    registeredComponents.insert(name_hash);
}

string PropertyRegistrator::GetClassName()
{
    return className;
}

uint PropertyRegistrator::GetCount()
{
    return (uint)registeredProperties.size();
}

Property* PropertyRegistrator::Get(uint property_index)
{
    if (property_index < (uint)registeredProperties.size())
        return registeredProperties[property_index];
    return nullptr;
}

Property* PropertyRegistrator::Find(const string& property_name)
{
    string key = property_name;
    size_t separator = key.find('.');
    if (separator != string::npos)
        key[separator] = '_';

    for (auto& prop : registeredProperties)
        if (prop->propName == key)
            return prop;
    return nullptr;
}

Property* PropertyRegistrator::FindByEnum(int enum_value)
{
    for (auto& prop : registeredProperties)
        if (prop->enumValue == enum_value)
            return prop;
    return nullptr;
}

bool PropertyRegistrator::IsComponentRegistered(hash component_name)
{
    return registeredComponents.count(component_name) > 0;
}

void PropertyRegistrator::SetNativeSetCallback(const string& property_name, NativeCallback callback)
{
    RUNTIME_ASSERT(!property_name.empty());
    // Find(property_name)->nativeSetCallback = callback;
}

uint PropertyRegistrator::GetWholeDataSize()
{
    return wholePodDataSize;
}
