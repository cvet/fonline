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

auto Property::CreateRefValue(uchar* /*data*/, uint /*data_size*/) -> unique_ptr<void, std::function<void(void*)>>
{
    /*if (dataType == Property::Array)
    {
        if (isArrayOfString)
        {
            if (data_size)
            {
                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(uint);
                CScriptArray* arr = CScriptArray::Create(asObjType, arr_size);
                RUNTIME_ASSERT(arr);
                for (uint i = 0; i < arr_size; i++)
                {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
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
                std::memcpy(arr->At(0), data, arr_size * element_size);
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
                    std::memcpy(&arr_size, data, sizeof(arr_size));
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
                                std::memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(uint);
                                string str((char*)data, str_size);
                                arr->SetValue(i, &str);
                                data += str_size;
                            }
                        }
                        else
                        {
                            std::memcpy(arr->At(0), data, arr_size * arr_element_size);
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
                    std::memcpy(&str_size, data, sizeof(str_size));
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

auto Property::ExpandComplexValueData(void* /*value*/, uint& /*data_size*/, bool& /*need_delete*/) -> uchar*
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
                std::memcpy(buf, &arr_size, sizeof(uint));
                buf += sizeof(uint);
                for (uint i = 0; i < arr_size; i++)
                {
                    string& str = *(string*)arr->At(i);
                    uint str_size = (uint)str.length();
                    std::memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (str_size)
                    {
                        std::memcpy(buf, str.c_str(), str_size);
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
                    std::memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    uint arr_size = (arr ? arr->GetSize() : 0);
                    std::memcpy(buf, &arr_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (arr_size)
                    {
                        if (isDictOfArrayOfString)
                        {
                            for (uint i = 0; i < arr_size; i++)
                            {
                                string& str = *(string*)arr->At(i);
                                uint str_size = (uint)str.length();
                                std::memcpy(buf, &str_size, sizeof(uint));
                                buf += sizeof(uint);
                                if (str_size)
                                {
                                    std::memcpy(buf, str.c_str(), str_size);
                                    buf += arr_size;
                                }
                            }
                        }
                        else
                        {
                            std::memcpy(buf, arr->At(0), arr_size * arr_element_size);
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
                    std::memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    uint str_size = (uint)str.length();
                    std::memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(uint);
                    if (str_size)
                    {
                        std::memcpy(buf, str.c_str(), str_size);
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
                    std::memcpy(buf, kv.first, key_element_size);
                    buf += key_element_size;
                    std::memcpy(buf, kv.second, value_element_size);
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

auto Property::GetName() const -> string
{
    return _propName;
}

auto Property::GetTypeName() const -> string
{
    return _typeName;
}

auto Property::GetRegIndex() const -> uint
{
    return _regIndex;
}

auto Property::GetEnumValue() const -> int
{
    return _enumValue;
}

auto Property::GetAccess() const -> AccessType
{
    return _accessType;
}

auto Property::GetBaseSize() const -> uint
{
    return _baseSize;
}

auto Property::IsPOD() const -> bool
{
    return _dataType == POD;
}

auto Property::IsDict() const -> bool
{
    return _dataType == Dict;
}

auto Property::IsHash() const -> bool
{
    return _isHash;
}

auto Property::IsResource() const -> bool
{
    return _isResource;
}

auto Property::IsEnum() const -> bool
{
    return _isEnumDataType;
}

auto Property::IsReadable() const -> bool
{
    return _isReadable;
}

auto Property::IsWritable() const -> bool
{
    return _isWritable;
}

auto Property::IsConst() const -> bool
{
    return _isConst;
}

auto Property::IsTemporary() const -> bool
{
    return _isTemporary;
}

auto Property::IsNoHistory() const -> bool
{
    return _isNoHistory;
}

Properties::Properties(PropertyRegistrator* reg)
{
    _registrator = reg;
    RUNTIME_ASSERT(_registrator);

    _sendIgnoreEntity = nullptr;
    _sendIgnoreProperty = nullptr;

    // Allocate POD data
    if (!_registrator->_podDataPool.empty()) {
        _podData = _registrator->_podDataPool.back();
        _registrator->_podDataPool.pop_back();
    }
    else {
        _podData = new uchar[_registrator->_wholePodDataSize];
    }

    std::memset(_podData, 0, _registrator->_wholePodDataSize);

    // Complex data
    _complexData.resize(_registrator->_complexPropertiesCount);
    _complexDataSizes.resize(_registrator->_complexPropertiesCount);
}

Properties::Properties(const Properties& other) : Properties(other._registrator)
{
    // Copy POD data
    std::memcpy(&_podData[0], &other._podData[0], _registrator->_wholePodDataSize);

    // Copy complex data
    for (size_t i = 0; i < other._complexData.size(); i++) {
        const auto size = other._complexDataSizes[i];
        _complexDataSizes[i] = size;
        _complexData[i] = size != 0u ? new uchar[size] : nullptr;
        if (size != 0u) {
            std::memcpy(_complexData[i], other._complexData[i], size);
        }
    }
}

Properties::~Properties()
{
    _registrator->_podDataPool.push_back(_podData);

    for (auto& cd : _complexData) {
        delete[] cd;
    }
}

auto Properties::operator=(const Properties& other) -> Properties&
{
    RUNTIME_ASSERT(_registrator == other._registrator);

    // Copy POD data
    std::memcpy(&_podData[0], &other._podData[0], _registrator->_wholePodDataSize);

    // Copy complex data
    for (auto* prop : _registrator->_registeredProperties) {
        if (prop->_complexDataIndex != static_cast<uint>(-1)) {
            SetRawData(prop, other._complexData[prop->_complexDataIndex], other._complexDataSizes[prop->_complexDataIndex]);
        }
    }

    return *this;
}

auto Properties::FindByEnum(int enum_value) -> Property*
{
    NON_CONST_METHOD_HINT();

    return _registrator->FindByEnum(enum_value);
}

auto Properties::FindData(const string& property_name) -> void*
{
    NON_CONST_METHOD_HINT();

    const auto* prop = _registrator->Find(property_name);
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));

    return prop != nullptr ? &_podData[prop->_podDataOffset] : nullptr;
}

auto Properties::StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint
{
    uint whole_size = 0;
    *all_data = &_storeData;
    *all_data_sizes = &_storeDataSizes;
    _storeData.resize(0);
    _storeDataSizes.resize(0);

    // Store POD properties data
    _storeData.push_back(_podData);
    _storeDataSizes.push_back(static_cast<uint>(_registrator->_publicPodDataSpace.size()) + (with_protected ? static_cast<uint>(_registrator->_protectedPodDataSpace.size()) : 0));
    whole_size += _storeDataSizes.back();

    // Calculate complex data to send
    _storeDataComplexIndicies = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;
    for (size_t i = 0; i < _storeDataComplexIndicies.size();) {
        auto* prop = _registrator->_registeredProperties[_storeDataComplexIndicies[i]];
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        if (_complexDataSizes[prop->_complexDataIndex] == 0u) {
            _storeDataComplexIndicies.erase(_storeDataComplexIndicies.begin() + i);
        }
        else {
            i++;
        }
    }

    // Store complex properties data
    if (!_storeDataComplexIndicies.empty()) {
        _storeData.push_back(reinterpret_cast<uchar*>(&_storeDataComplexIndicies[0]));
        _storeDataSizes.push_back(static_cast<uint>(_storeDataComplexIndicies.size()) * sizeof(short));
        whole_size += _storeDataSizes.back();

        for (auto index : _storeDataComplexIndicies) {
            auto* prop = _registrator->_registeredProperties[index];
            _storeData.push_back(_complexData[prop->_complexDataIndex]);
            _storeDataSizes.push_back(_complexDataSizes[prop->_complexDataIndex]);
            whole_size += _storeDataSizes.back();
        }
    }

    return whole_size;
}

void Properties::RestoreData(vector<uchar*>& all_data, vector<uint>& all_data_sizes)
{
    // Restore POD data
    const auto public_size = static_cast<uint>(_registrator->_publicPodDataSpace.size());
    const auto protected_size = static_cast<uint>(_registrator->_protectedPodDataSpace.size());
    RUNTIME_ASSERT(all_data_sizes[0] == public_size || all_data_sizes[0] == public_size + protected_size);
    if (all_data_sizes[0] != 0u) {
        std::memcpy(_podData, all_data[0], all_data_sizes[0]);
    }

    // Restore complex data
    if (all_data.size() > 1) {
        const uint comlplex_data_count = all_data_sizes[1] / sizeof(ushort);
        RUNTIME_ASSERT(comlplex_data_count > 0);
        vector<ushort> complex_indicies(comlplex_data_count);
        std::memcpy(&complex_indicies[0], all_data[1], all_data_sizes[1]);

        for (size_t i = 0; i < complex_indicies.size(); i++) {
            RUNTIME_ASSERT(complex_indicies[i] < _registrator->_registeredProperties.size());
            auto* prop = _registrator->_registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            const auto data_size = all_data_sizes[2 + i];
            auto* data = all_data[2 + i];
            SetRawData(prop, data, data_size);
        }
    }
}

void Properties::RestoreData(vector<vector<uchar>>& all_data)
{
    vector<uchar*> all_data_ext(all_data.size());
    vector<uint> all_data_sizes(all_data.size());
    for (size_t i = 0; i < all_data.size(); i++) {
        all_data_ext[i] = !all_data[i].empty() ? &all_data[i][0] : nullptr;
        all_data_sizes[i] = static_cast<uint>(all_data[i].size());
    }
    RestoreData(all_data_ext, all_data_sizes);
}

static auto ReadToken(const char* str, string& result) -> const char*
{
    if (*str == 0) {
        return nullptr;
    }

    const char* begin;
    const auto* s = str;

    uint length = 0;
    utf8::Decode(s, &length);
    while (length == 1 && (*s == ' ' || *s == '\t')) {
        utf8::Decode(++s, &length);
    }

    if (*s == 0) {
        return nullptr;
    }

    if (length == 1 && *s == '{') {
        s++;
        begin = s;

        auto braces = 1;
        while (*s != 0) {
            if (length == 1 && *s == '\\') {
                ++s;
                if (*s != 0) {
                    utf8::Decode(s, &length);
                    s += length;
                }
            }
            else if (length == 1 && *s == '{') {
                braces++;
                s++;
            }
            else if (length == 1 && *s == '}') {
                braces--;
                if (braces == 0) {
                    break;
                }
                s++;
            }
            else {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }
    else {
        begin = s;
        while (*s != 0) {
            if (length == 1 && *s == '\\') {
                utf8::Decode(++s, &length);
                s += length;
            }
            else if (length == 1 && (*s == ' ' || *s == '\t')) {
                break;
            }
            else {
                s += length;
            }
            utf8::Decode(s, &length);
        }
    }

    result.assign(begin, static_cast<uint>(s - begin));
    return *s != 0 ? s + 1 : s;
}

static auto CodeString(const string& str, int deep) -> string
{
    auto need_braces = false;
    if (deep > 0 && (str.empty() || str.find_first_of(" \t") != string::npos)) {
        need_braces = true;
    }
    if (!need_braces && deep == 0 && !str.empty() && (str.find_first_of(" \t") == 0 || str.find_last_of(" \t") == str.length() - 1)) {
        need_braces = true;
    }
    if (!need_braces && str.length() >= 2 && str.back() == '\\' && str[str.length() - 2] == ' ') {
        need_braces = true;
    }

    string result;
    result.reserve(str.length() * 2);

    if (need_braces) {
        result.append("{");
    }

    const auto* s = str.c_str();
    uint length = 0;
    while (*s != 0) {
        utf8::Decode(s, &length);
        if (length == 1) {
            switch (*s) {
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
        else {
            result.append(s, length);
        }

        s += length;
    }

    if (need_braces) {
        result.append("}");
    }

    return result;
}

static auto DecodeString(const string& str) -> string
{
    if (str.empty()) {
        return str;
    }

    string result;
    result.reserve(str.length());

    const auto* s = str.c_str();
    uint length = 0;

    utf8::Decode(s, &length);
    const auto is_braces = length == 1 && *s == '{';
    if (is_braces) {
        s++;
    }

    while (*s != 0) {
        utf8::Decode(s, &length);
        if (length == 1 && *s == '\\') {
            utf8::Decode(++s, &length);

            switch (*s) {
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
        else {
            result.append(s, length);
        }

        s += length;
    }

    if (is_braces && length == 1 && result.back() == '}') {
        result.pop_back();
    }

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
        std::memcpy(pod_buf, &v, sizeof(ctype)); \
        return pod_buf; \
    }

        if (is_hashes[deep])
        {
            RUNTIME_ASSERT(type_id == asTYPEID_UINT32);
            hash v = _str(DecodeString(value)).toHash();
            std::memcpy(pod_buf, &v, sizeof(v));
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
        // std::memcpy(pod_buf, &v, sizeof(v));
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

auto Properties::LoadFromText(const map<string, string>& /*key_values*/) -> bool
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

auto Properties::SaveToText(Properties* base) const -> map<string, string>
{
    RUNTIME_ASSERT(!base || _registrator == base->_registrator);

    map<string, string> key_values;
    for (auto* prop : _registrator->_registeredProperties) {
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
                if (memcmp(&_podData[prop->_podDataOffset], &base->_podData[prop->_podDataOffset], prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (_complexDataSizes[prop->_complexDataIndex] == 0u && base->_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
                if (_complexDataSizes[prop->_complexDataIndex] == base->_complexDataSizes[prop->_complexDataIndex] && memcmp(_complexData[prop->_complexDataIndex], base->_complexData[prop->_complexDataIndex], _complexDataSizes[prop->_complexDataIndex]) == 0) {
                    continue;
                }
            }
        }
        else {
            if (prop->_podDataOffset != static_cast<uint>(-1)) {
                uint64 pod_zero = 0;
                RUNTIME_ASSERT(prop->_baseSize <= sizeof(pod_zero));
                if (memcmp(&_podData[prop->_podDataOffset], &pod_zero, prop->_baseSize) == 0) {
                    continue;
                }
            }
            else {
                if (_complexDataSizes[prop->_complexDataIndex] == 0u) {
                    continue;
                }
            }
        }

        // Serialize to text and store in map
        key_values.insert(std::make_pair(prop->_propName, SavePropertyToText(prop)));
    }

    return key_values;
}

auto Properties::LoadPropertyFromText(Property* /*prop*/, const string& /*text*/) -> bool
{
    /*RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(registrator == prop->registrator);
    RUNTIME_ASSERT(prop->podDataOffset != uint(-1) || prop->complexDataIndex != uint(-1));
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

auto Properties::SavePropertyToText(Property* prop) const -> string
{
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1) || prop->_complexDataIndex != static_cast<uint>(-1));

    uint data_size = 0;
    // void* data = GetRawData(prop, data_size);

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
    if (prop != nullptr) {
        RUNTIME_ASSERT(_sendIgnoreEntity == nullptr);
        RUNTIME_ASSERT(_sendIgnoreProperty == nullptr);
    }
    else {
        RUNTIME_ASSERT(_sendIgnoreEntity != nullptr);
        RUNTIME_ASSERT(_sendIgnoreProperty != nullptr);
    }

    _sendIgnoreEntity = entity;
    _sendIgnoreProperty = prop;
}

auto Properties::GetRawDataSize(Property* prop) const -> uint
{
    uint data_size = 0;
    const auto* data = GetRawData(prop, data_size);
    UNUSED_VARIABLE(data);
    return data_size;
}

auto Properties::GetRawData(Property* prop, uint& data_size) const -> const uchar*
{
    if (prop->_dataType == Property::POD) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        data_size = prop->_baseSize;
        return &_podData[prop->_podDataOffset];
    }

    RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    data_size = _complexDataSizes[prop->_complexDataIndex];
    return _complexData[prop->_complexDataIndex];
}

auto Properties::GetRawData(Property* prop, uint& data_size) -> uchar*
{
    return const_cast<uchar*>(const_cast<const Properties*>(this)->GetRawData(prop, data_size));
}

void Properties::SetRawData(Property* prop, uchar* data, uint data_size)
{
    if (prop->IsPOD()) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        RUNTIME_ASSERT(prop->_baseSize == data_size);

        std::memcpy(&_podData[prop->_podDataOffset], data, data_size);
    }
    else {
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));

        if (data_size != _complexDataSizes[prop->_complexDataIndex]) {
            _complexDataSizes[prop->_complexDataIndex] = data_size;
            delete[] _complexData[prop->_complexDataIndex];
            if (data_size != 0u) {
                _complexData[prop->_complexDataIndex] = new uchar[data_size];
            }
        }

        if (data_size != 0u) {
            std::memcpy(_complexData[prop->_complexDataIndex], data, data_size);
        }
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

auto Properties::GetPODValueAsInt(Property* prop) const -> int
{
    RUNTIME_ASSERT(prop->_dataType == Property::POD);

    if (prop->_isBoolDataType) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    if (prop->_isFloatDataType) {
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<float>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<double>(prop));
        }
    }
    else if (prop->_isSignedIntDataType) {
        if (prop->_baseSize == 1) {
            return static_cast<int>(GetValue<char>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<short>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<int>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<int64>(prop));
        }
    }
    else {
        if (prop->_baseSize == 1) {
            return static_cast<int>(GetValue<uchar>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<int>(GetValue<ushort>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<uint>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<uint64>(prop));
        }
    }

    throw UnreachablePlaceException(LINE_STR);
}

void Properties::SetPODValueAsInt(Property* prop, int value)
{
    RUNTIME_ASSERT(prop->_dataType == Property::POD);

    if (prop->_isBoolDataType) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloatDataType) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isSignedIntDataType) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<short>(prop, static_cast<short>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, value);
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else {
        if (prop->_baseSize == 1) {
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<uint64>(prop, static_cast<uint64>(value));
        }
    }
}

auto Properties::GetValueAsInt(int enum_value) const -> int
{
    auto* prop = _registrator->FindByEnum(enum_value);
    if (prop == nullptr) {
        throw PropertiesException("Enum not found", enum_value);
    }
    if (!prop->IsPOD()) {
        throw PropertiesException("Can't retreive integer value from non POD property", prop->GetName());
    }
    if (!prop->IsReadable()) {
        throw PropertiesException("Can't retreive integer value from non readable property", prop->GetName());
    }

    return GetPODValueAsInt(prop);
}

void Properties::SetValueAsInt(int enum_value, int value)
{
    auto* prop = _registrator->FindByEnum(enum_value);
    if (prop == nullptr) {
        throw PropertiesException("Enum not found", enum_value);
    }
    if (!prop->IsPOD()) {
        throw PropertiesException("Can't set integer value to non POD property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPODValueAsInt(prop, value);
}

void Properties::SetValueAsIntByName(const string& enum_name, int value)
{
    auto* prop = _registrator->Find(enum_name);
    if (prop == nullptr) {
        throw PropertiesException("Enum not found", enum_name);
    }
    if (!prop->IsPOD()) {
        throw PropertiesException("Can't set by name integer value from non POD property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPODValueAsInt(prop, value);
}

void Properties::SetValueAsIntProps(int enum_value, int value)
{
    auto* prop = _registrator->FindByEnum(enum_value);
    if (prop == nullptr) {
        throw PropertiesException("Enum not found", enum_value);
    }
    if (!prop->IsPOD()) {
        throw PropertiesException("Can't set integer value to non POD property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }
    if ((prop->_accessType & Property::VirtualMask) != 0) {
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());
    }

    if (prop->_isBoolDataType) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloatDataType) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isSignedIntDataType) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<short>(prop, static_cast<short>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, value);
        }
        else if (prop->_baseSize == 8) {
            SetValue<int64>(prop, static_cast<int64>(value));
        }
    }
    else {
        if (prop->_baseSize == 1) {
            SetValue<uchar>(prop, static_cast<uchar>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<ushort>(prop, static_cast<ushort>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<uint>(prop, static_cast<uint>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<uint64>(prop, static_cast<uint64>(value));
        }
    }
}

PropertyRegistrator::PropertyRegistrator(bool is_server)
{
    _isServer = is_server;
}

PropertyRegistrator::~PropertyRegistrator()
{
    for (auto& prop : _registeredProperties) {
        delete prop;
    }
    for (auto& data : _podDataPool) {
        delete[] data;
    }
}

auto PropertyRegistrator::Register(Property::AccessType access, const type_info& type, const string& name) -> Property*
{
#define ISTYPE(t) (type.hash_code() == typeid(t).hash_code())

    auto data_type = Property::DataType::POD;
    uint data_size = 0;
    auto is_int_data_type = false;
    auto is_signed_int_data_type = false;
    auto is_float_data_type = false;
    auto is_bool_data_type = false;
    const auto is_enum_data_type = false;
    auto is_array_of_string = false;
    const auto is_dict_of_string = false;
    const auto is_dict_of_array = false;
    const auto is_dict_of_array_of_string = false;
    auto is_hash_sub0 = false;
    const auto is_hash_sub1 = false;
    const auto is_hash_sub2 = false;

    if (ISTYPE(int) || ISTYPE(uint) || ISTYPE(char) || ISTYPE(uchar) || ISTYPE(short) || ISTYPE(ushort) || ISTYPE(int64) || ISTYPE(uint64) || ISTYPE(float) || ISTYPE(double) || ISTYPE(bool)) {
        data_type = Property::POD;

        if (ISTYPE(char) || ISTYPE(uchar) || ISTYPE(bool)) {
            data_size = 1;
        }
        else if (ISTYPE(short) || ISTYPE(ushort)) {
            data_size = 2;
        }
        else if (ISTYPE(int) || ISTYPE(uint) || ISTYPE(float)) {
            data_size = 4;
        }
        else if (ISTYPE(int64) || ISTYPE(uint64) || ISTYPE(double)) {
            data_size = 8;
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }

        is_int_data_type = ISTYPE(int) || ISTYPE(uint) || ISTYPE(char) || ISTYPE(uchar) || ISTYPE(short) || ISTYPE(ushort) || ISTYPE(int64) || ISTYPE(uint64);
        is_signed_int_data_type = ISTYPE(int) || ISTYPE(char) || ISTYPE(short) || ISTYPE(int64);
        is_float_data_type = ISTYPE(float) || ISTYPE(double);
        is_bool_data_type = ISTYPE(bool);
        // is_enum_data_type = (type_id > asTYPEID_DOUBLE);
    }
    else if (ISTYPE(string)) {
        data_type = Property::String;
        data_size = sizeof(string);
    }
    else if (ISTYPE(vector<int>)) {
        data_type = Property::Array;
        data_size = sizeof(void*);

        auto is_array_of_pod = ISTYPE(vector<int>);
        is_array_of_string = ISTYPE(vector<string>);
        is_hash_sub0 = ISTYPE(vector<hash>);
    }
    else if (ISTYPE(vector<int>)) {
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
    else {
        // WriteLog("Invalid property type '{}'.\n", type_name);
        // return nullptr;
    }

    // Allocate property
    auto* prop = new Property();
    const auto reg_index = static_cast<uint>(_registeredProperties.size());

    // Disallow set or get accessors
    auto disable_get = false;
    auto disable_set = false;
    if (_isServer && (access & Property::ClientOnlyMask) != 0) {
        disable_get = disable_set = true;
    }
    if (!_isServer && (access & Property::ServerOnlyMask) != 0) {
        disable_get = disable_set = true;
    }
    if (!_isServer && ((access & Property::PublicMask) != 0 || (access & Property::ProtectedMask) != 0) && (access & Property::ModifiableMask) == 0) {
        disable_set = true;
    }
    // if (is_const) {
    //    disable_set = true;
    //}

    // Register default getter
    if (disable_get) {
        return nullptr;
    }

    // Register setter
    if (!disable_set) {
    }

    // POD property data offset
    auto data_base_offset = static_cast<uint>(-1);
    if (data_type == Property::POD && !disable_get && (access & Property::VirtualMask) == 0) {
        const auto is_public = (access & Property::PublicMask) != 0;
        const auto is_protected = (access & Property::ProtectedMask) != 0;
        auto& space = is_public ? _publicPodDataSpace : is_protected ? _protectedPodDataSpace : _privatePodDataSpace;

        const auto space_size = static_cast<uint>(space.size());
        uint space_pos = 0;
        while (space_pos + data_size <= space_size) {
            auto fail = false;
            for (uint i = 0; i < data_size; i++) {
                if (space[space_pos + i]) {
                    fail = true;
                    break;
                }
            }
            if (!fail) {
                break;
            }
            space_pos += data_size;
        }

        auto new_size = space_pos + data_size;
        new_size += 8 - new_size % 8;
        if (new_size > static_cast<uint>(space.size())) {
            space.resize(new_size);
        }

        for (uint i = 0; i < data_size; i++) {
            space[space_pos + i] = true;
        }

        data_base_offset = space_pos;

        _wholePodDataSize = static_cast<uint>(_publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size());
        // RUNTIME_ASSERT((wholePodDataSize % 8) == 0);
    }

    // Complex property data index
    auto complex_data_index = static_cast<uint>(-1);
    if (data_type != Property::POD && (!disable_get || !disable_set) && (access & Property::VirtualMask) == 0) {
        complex_data_index = _complexPropertiesCount++;
        if ((access & Property::PublicMask) != 0) {
            _publicComplexDataProps.push_back(static_cast<ushort>(reg_index));
            _publicProtectedComplexDataProps.push_back(static_cast<ushort>(reg_index));
        }
        else if ((access & Property::ProtectedMask) != 0) {
            _protectedComplexDataProps.push_back(static_cast<ushort>(reg_index));
            _publicProtectedComplexDataProps.push_back(static_cast<ushort>(reg_index));
        }
        else if ((access & Property::PrivateMask) != 0) {
            _privateComplexDataProps.push_back(static_cast<ushort>(reg_index));
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    // Make entry
    prop->_registrator = this;
    prop->_regIndex = reg_index;
    prop->_getIndex = !disable_get ? _getPropertiesCount++ : static_cast<uint>(-1);
    // prop->enumValue = enum_value;
    prop->_complexDataIndex = complex_data_index;
    prop->_podDataOffset = data_base_offset;
    prop->_baseSize = data_size;

    prop->_propName = _str(name).replace('.', '_');
    // prop->typeName = type_name;
    // prop->componentName = component_name;
    prop->_dataType = data_type;
    prop->_accessType = access;
    // prop->isConst = is_const;
    // prop->asObjTypeId = type_id;
    // prop->asObjType = as_obj_type;
    // prop->isHash = type_name == "hash";
    prop->_isHashSubType0 = is_hash_sub0;
    prop->_isHashSubType1 = is_hash_sub1;
    prop->_isHashSubType2 = is_hash_sub2;
    // prop->isResource = type_name == "resource";
    prop->_isIntDataType = is_int_data_type;
    prop->_isSignedIntDataType = is_signed_int_data_type;
    prop->_isFloatDataType = is_float_data_type;
    prop->_isEnumDataType = is_enum_data_type;
    prop->_isBoolDataType = is_bool_data_type;
    prop->_isArrayOfString = is_array_of_string;
    prop->_isDictOfString = is_dict_of_string;
    prop->_isDictOfArray = is_dict_of_array;
    prop->_isDictOfArrayOfString = is_dict_of_array_of_string;
    prop->_isReadable = !disable_get;
    prop->_isWritable = !disable_set;
    // prop->isTemporary = (defaultTemporary || is_temporary);
    // prop->isNoHistory = (defaultNoHistory || is_no_history);
    // prop->checkMinValue = (min_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->checkMaxValue = (max_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->minValue = (min_value ? *min_value : 0);
    // prop->maxValue = (max_value ? *max_value : 0);

    _registeredProperties.push_back(prop);

    // Fix POD data offsets
    for (auto* prop : _registeredProperties) {
        if (prop->_podDataOffset == static_cast<uint>(-1)) {
            continue;
        }

        if ((prop->_accessType & Property::ProtectedMask) != 0) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size());
        }
        else if ((prop->_accessType & Property::PrivateMask) != 0) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size()) + static_cast<uint>(_protectedPodDataSpace.size());
        }
    }

    return prop;
}

void PropertyRegistrator::RegisterComponent(const string& name)
{
    const auto name_hash = _str(name).toHash();
    RUNTIME_ASSERT(!_registeredComponents.count(name_hash));
    _registeredComponents.insert(name_hash);
}

auto PropertyRegistrator::GetClassName() const -> string
{
    return _className;
}

auto PropertyRegistrator::GetCount() const -> uint
{
    return static_cast<uint>(_registeredProperties.size());
}

auto PropertyRegistrator::Get(uint property_index) -> Property*
{
    if (property_index < static_cast<uint>(_registeredProperties.size())) {
        return _registeredProperties[property_index];
    }
    return nullptr;
}

auto PropertyRegistrator::Find(const string& property_name) -> Property*
{
    auto key = property_name;
    const auto separator = key.find('.');
    if (separator != string::npos) {
        key[separator] = '_';
    }

    for (auto& prop : _registeredProperties) {
        if (prop->_propName == key) {
            return prop;
        }
    }
    return nullptr;
}

auto PropertyRegistrator::FindByEnum(int enum_value) -> Property*
{
    for (auto& prop : _registeredProperties) {
        if (prop->_enumValue == enum_value) {
            return prop;
        }
    }
    return nullptr;
}

auto PropertyRegistrator::IsComponentRegistered(hash component_name) const -> bool
{
    return _registeredComponents.count(component_name) > 0;
}

void PropertyRegistrator::SetNativeSetCallback(const string& property_name, const NativeCallback& /*callback*/)
{
    RUNTIME_ASSERT(!property_name.empty());
    // Find(property_name)->nativeSetCallback = callback;
}

auto PropertyRegistrator::GetWholeDataSize() const -> uint
{
    return _wholePodDataSize;
}
