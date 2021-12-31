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

Property::Property(const PropertyRegistrator* registrator) : _registrator {registrator}
{
}

auto Property::CreateRefValue(uchar* /*data*/, uint /*data_size*/) -> unique_ptr<void, std::function<void(void*)>>
{
    return nullptr;
}

auto Property::ExpandComplexValueData(void* /*value*/, uint& /*data_size*/, bool& /*need_delete*/) -> uchar*
{
    return nullptr;
}

Properties::Properties(const PropertyRegistrator* registrator) : _registrator {registrator}
{
    RUNTIME_ASSERT(_registrator);

    _sendIgnoreEntity = nullptr;
    _sendIgnoreProperty = nullptr;

    // Allocate plain data
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
    // Copy PlainData data
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
    // Hide warning about throwing desctructor
    try {
        _registrator->_podDataPool.push_back(_podData);
    }
    catch (...) {
    }

    for (const auto* cd : _complexData) {
        delete[] cd;
    }
}

auto Properties::operator=(const Properties& other) -> Properties&
{
    if (this == &other) {
        return *this;
    }

    RUNTIME_ASSERT(_registrator == other._registrator);

    // Copy PlainData data
    std::memcpy(&_podData[0], &other._podData[0], _registrator->_wholePodDataSize);

    // Copy complex data
    for (const auto* prop : _registrator->_registeredProperties) {
        if (prop->_complexDataIndex != static_cast<uint>(-1)) {
            SetRawData(prop, other._complexData[prop->_complexDataIndex], other._complexDataSizes[prop->_complexDataIndex]);
        }
    }

    return *this;
}

auto Properties::StoreData(bool with_protected, vector<uchar*>** all_data, vector<uint>** all_data_sizes) const -> uint
{
    uint whole_size = 0u;
    *all_data = &_storeData;
    *all_data_sizes = &_storeDataSizes;
    _storeData.resize(0u);
    _storeDataSizes.resize(0u);

    const auto preserve_size = 1u + (!_storeDataComplexIndicies.empty() ? 1u + _storeDataComplexIndicies.size() : 0u);
    _storeData.reserve(preserve_size);
    _storeDataSizes.reserve(preserve_size);

    // Store plain properties data
    _storeData.push_back(_podData);
    _storeDataSizes.push_back(static_cast<uint>(_registrator->_publicPodDataSpace.size()) + (with_protected ? static_cast<uint>(_registrator->_protectedPodDataSpace.size()) : 0));
    whole_size += _storeDataSizes.back();

    // Calculate complex data to send
    _storeDataComplexIndicies = with_protected ? _registrator->_publicProtectedComplexDataProps : _registrator->_publicComplexDataProps;
    for (size_t i = 0; i < _storeDataComplexIndicies.size();) {
        const auto* prop = _registrator->_registeredProperties[_storeDataComplexIndicies[i]];
        RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
        if (_complexDataSizes[prop->_complexDataIndex] == 0u) {
            _storeDataComplexIndicies.erase(_storeDataComplexIndicies.begin() + static_cast<int>(i));
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

        for (const auto index : xrange(_storeDataComplexIndicies)) {
            const auto* prop = _registrator->_registeredProperties[index];
            _storeData.push_back(_complexData[prop->_complexDataIndex]);
            _storeDataSizes.push_back(_complexDataSizes[prop->_complexDataIndex]);
            whole_size += _storeDataSizes.back();
        }
    }

    return whole_size;
}

void Properties::RestoreData(const vector<const uchar*>& all_data, const vector<uint>& all_data_sizes)
{
    // Restore PlainData data
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
            const auto* prop = _registrator->_registeredProperties[complex_indicies[i]];
            RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
            const auto data_size = all_data_sizes[2 + i];
            const auto* data = all_data[2 + i];
            SetRawData(prop, data, data_size);
        }
    }
}

void Properties::RestoreData(const vector<vector<uchar>>& all_data)
{
    vector<const uchar*> all_data_ext(all_data.size());
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

static auto CodeString(string_view str, int deep) -> string
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

    const auto* s = str.data();
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

static auto DecodeString(string_view str) -> string
{
    if (str.empty()) {
        return string();
    }

    string result;
    result.reserve(str.length());

    const auto* s = str.data();
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
        // return Script::ResolveEnumValueName(Script::GetEngine()->GetTypeDeclaration(type_id), VALUE_AS(int));
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

        // int v = Script::ResolveEnumValue(Script::GetEngine()->GetTypeDeclaration(type_id), value, is_error);
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

auto Properties::LoadFromText(const map<string, string>& key_values) -> bool
{
    bool is_error = false;

    for (const auto& [key, value] : key_values) {
        // Skip technical fields
        if (key.empty() || key[0] == '$' || key[0] == '_') {
            continue;
        }

        // Find property
        const auto* prop = _registrator->Find(key);
        if (prop == nullptr || (prop->_podDataOffset == static_cast<uint>(-1) && prop->_complexDataIndex == static_cast<uint>(-1))) {
            if (prop == nullptr) {
                WriteLog("Unknown property '{}'.\n", key);
            }
            else {
                WriteLog("Invalid property '{}' for reading.\n", prop->GetName());
            }

            is_error = true;
            continue;
        }

        // Parse
        if (!LoadPropertyFromText(prop, value)) {
            is_error = true;
        }
    }

    return !is_error;
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

auto Properties::LoadPropertyFromText(const Property* prop, string_view text) -> bool
{
    RUNTIME_ASSERT(prop);
    RUNTIME_ASSERT(_registrator == prop->_registrator);
    RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1) || prop->_complexDataIndex != static_cast<uint>(-1));

    bool is_error = false;

    /*// Parse
    uchar pod_buf[8];
    bool is_hashes[] = { prop->_isHash || prop->_isResource, prop->_isHashSubType0, prop->_isHashSubType1, prop->_isHashSubType2};
    void* value = ReadValue(text, prop->asObjTypeId, prop->asObjType, is_hashes, 0, pod_buf, is_error);

    // Assign
    if (prop->_podDataOffset != static_cast<uint>(-1))
    {
        RUNTIME_ASSERT(value == pod_buf);
        SetRawData(prop, pod_buf, prop->_baseSize);
    }
    else if (prop->_complexDataIndex != static_cast<uint>(-1))
    {
        bool need_delete;
        uint data_size;
        uchar* data = prop->ExpandComplexValueData(value, data_size, need_delete);
        SetRawData(prop, data, data_size);

        if (need_delete) {
            delete[] data;
        }
        // Script::GetEngine()->ReleaseScriptObject(value, prop->asObjType);
    }*/

    return !is_error;
}

auto Properties::SavePropertyToText(const Property* prop) const -> string
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

void Properties::SetSendIgnore(const Property* prop, const Entity* entity)
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

auto Properties::GetRawDataSize(const Property* prop) const -> uint
{
    uint data_size = 0;
    const auto* data = GetRawData(prop, data_size);
    UNUSED_VARIABLE(data);
    return data_size;
}

auto Properties::GetRawData(const Property* prop, uint& data_size) const -> const uchar*
{
    if (prop->_dataType == Property::DataType::PlainData) {
        RUNTIME_ASSERT(prop->_podDataOffset != static_cast<uint>(-1));
        data_size = prop->_baseSize;
        return &_podData[prop->_podDataOffset];
    }

    RUNTIME_ASSERT(prop->_complexDataIndex != static_cast<uint>(-1));
    data_size = _complexDataSizes[prop->_complexDataIndex];
    return _complexData[prop->_complexDataIndex];
}

auto Properties::GetRawData(const Property* prop, uint& data_size) -> uchar*
{
    return const_cast<uchar*>(const_cast<const Properties*>(this)->GetRawData(prop, data_size));
}

void Properties::SetRawData(const Property* prop, const uchar* data, uint data_size)
{
    if (prop->IsPlainData()) {
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

void Properties::SetValueFromData(const Property* prop, const uchar* data, uint data_size)
{
    /*if (dataType == Property::String)
    {
        string str;
        if (data_size)
            str.assign((char*)data, data_size);
        GenericSet(entity, &str);
    }
    else if (dataType == Property::PlainData)
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

auto Properties::GetPlainDataValueAsInt(const Property* prop) const -> int
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return GetValue<bool>(prop) ? 1 : 0;
    }
    if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            return static_cast<int>(GetValue<float>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<int>(GetValue<double>(prop));
        }
    }
    else if (prop->_isSignedInt) {
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

auto Properties::GetPlainDataValueAsFloat(const Property* prop) const -> float
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        return GetValue<bool>(prop) ? 1.0f : 0.0f;
    }
    if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            return GetValue<float>(prop);
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<double>(prop));
        }
    }
    else if (prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            return static_cast<float>(GetValue<char>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<float>(GetValue<short>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<float>(GetValue<int>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<int64>(prop));
        }
    }
    else {
        if (prop->_baseSize == 1) {
            return static_cast<float>(GetValue<uchar>(prop));
        }
        if (prop->_baseSize == 2) {
            return static_cast<float>(GetValue<ushort>(prop));
        }
        if (prop->_baseSize == 4) {
            return static_cast<float>(GetValue<uint>(prop));
        }
        if (prop->_baseSize == 8) {
            return static_cast<float>(GetValue<uint64>(prop));
        }
    }

    throw UnreachablePlaceException(LINE_STR);
}

void Properties::SetPlainDataValueAsInt(const Property* prop, int value)
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isSignedInt) {
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

void Properties::SetPlainDataValueAsFloat(const Property* prop, float value)
{
    RUNTIME_ASSERT(prop->_dataType == Property::DataType::PlainData);

    if (prop->_isBool) {
        SetValue<bool>(prop, value != 0.0f);
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, value);
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isSignedInt) {
        if (prop->_baseSize == 1) {
            SetValue<char>(prop, static_cast<char>(value));
        }
        else if (prop->_baseSize == 2) {
            SetValue<short>(prop, static_cast<short>(value));
        }
        else if (prop->_baseSize == 4) {
            SetValue<int>(prop, static_cast<int>(value));
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

auto Properties::GetValueAsInt(int property_index) const -> int
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Enum not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't retreive integer value from non PlainData property", prop->GetName());
    }
    if (!prop->IsReadable()) {
        throw PropertiesException("Can't retreive integer value from non readable property", prop->GetName());
    }

    return GetPlainDataValueAsInt(prop);
}

void Properties::SetValueAsInt(int property_index, int value)
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Enum not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPlainDataValueAsInt(prop, value);
}

void Properties::SetValueAsIntByName(string_view property_name, int value)
{
    const auto* prop = _registrator->Find(property_name);

    if (prop == nullptr) {
        throw PropertiesException("Enum not found", property_name);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set by name integer value from non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }

    SetPlainDataValueAsInt(prop, value);
}

void Properties::SetValueAsIntProps(int property_index, int value)
{
    const auto* prop = _registrator->GetByIndex(property_index);

    if (prop == nullptr) {
        throw PropertiesException("Enum not found", property_index);
    }
    if (!prop->IsPlainData()) {
        throw PropertiesException("Can't set integer value to non PlainData property", prop->GetName());
    }
    if (!prop->IsWritable()) {
        throw PropertiesException("Can't set integer value to non writable property", prop->GetName());
    }
    if (IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
        throw PropertiesException("Can't set integer value to virtual property", prop->GetName());
    }

    if (prop->_isBool) {
        SetValue<bool>(prop, value != 0);
    }
    else if (prop->_isFloat) {
        if (prop->_baseSize == 4) {
            SetValue<float>(prop, static_cast<float>(value));
        }
        else if (prop->_baseSize == 8) {
            SetValue<double>(prop, static_cast<double>(value));
        }
    }
    else if (prop->_isSignedInt) {
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

PropertyRegistrator::PropertyRegistrator(string_view class_name, bool is_server) : _className {class_name}, _isServer {is_server}
{
}

PropertyRegistrator::~PropertyRegistrator()
{
    for (const auto* prop : _registeredProperties) {
        delete prop;
    }
    for (const auto* data : _podDataPool) {
        delete[] data;
    }
}

void PropertyRegistrator::RegisterComponent(hstring name)
{
    RUNTIME_ASSERT(!_registeredComponents.count(name));
    _registeredComponents.insert(name);
}

auto PropertyRegistrator::GetClassName() const -> const string&
{
    return _className;
}

auto PropertyRegistrator::GetCount() const -> uint
{
    return static_cast<uint>(_registeredProperties.size());
}

auto PropertyRegistrator::GetByIndex(int property_index) const -> const Property*
{
    if (property_index >= 0 && property_index < static_cast<int>(_registeredProperties.size())) {
        return _registeredProperties[property_index];
    }
    return nullptr;
}

auto PropertyRegistrator::Find(string_view property_name) const -> const Property*
{
    auto key = string(property_name);
    if (const auto separator = property_name.find('.'); separator != string::npos) {
        key[separator] = '_';
    }

    if (const auto it = _registeredPropertiesLookup.find(key); it != _registeredPropertiesLookup.end()) {
        return it->second;
    }

    return nullptr;
}

auto PropertyRegistrator::IsComponentRegistered(hstring component_name) const -> bool
{
    return _registeredComponents.count(component_name) > 0u;
}

void PropertyRegistrator::SetNativeSetCallback(string_view property_name, const NativeCallback& /*callback*/)
{
    RUNTIME_ASSERT(!property_name.empty());
    // Find(property_name)->nativeSetCallback = callback;
}

auto PropertyRegistrator::GetWholeDataSize() const -> uint
{
    return _wholePodDataSize;
}

void PropertyRegistrator::AppendProperty(Property* prop, const vector<string>& flags)
{
    // string _propName {};
    // string _componentName {};
    prop->_propName = _str(prop->_propName).replace('.', '_');

    if (prop->_isEnum) {
        prop->_baseTypeName = "";
    }
    if (prop->_dataType == Property::DataType::Dict && prop->_isDictKeyEnum) {
        prop->_dictKeyTypeName = "";
    }

    for (size_t i = 0; i < flags.size(); i++) {
        if (flags[i] == "Enum") {
            RUNTIME_ASSERT(prop->_isEnum);
            RUNTIME_ASSERT(flags[i + 1] == "=");
            prop->_baseTypeName = flags[i + 2];
            i += 2;
        }
        else if (flags[i] == "KeyEnum") {
            RUNTIME_ASSERT(prop->_dataType == Property::DataType::Dict);
            RUNTIME_ASSERT(flags[i + 1] == "=");
            prop->_dictKeyTypeName = flags[i + 2];
            i += 2;
        }
    }

    if (prop->_dataType == Property::DataType::Array) {
        prop->_fullTypeName = prop->_baseTypeName + "[]";
    }
    else if (prop->_dataType == Property::DataType::Dict) {
        if (prop->_isDictOfArray) {
            prop->_fullTypeName = "dict<" + prop->_dictKeyTypeName + ", " + prop->_baseTypeName + "[]>";
        }
        else {
            prop->_fullTypeName = "dict<" + prop->_baseTypeName + ", " + prop->_baseTypeName + ">";
        }
    }
    else {
        prop->_fullTypeName = prop->_baseTypeName;
    }

    RUNTIME_ASSERT(prop->_baseTypeName != "");
    if (prop->_dataType == Property::DataType::Dict) {
        RUNTIME_ASSERT(prop->_dictKeyTypeName != "");
    }

    // bool _isTemporary {};
    // bool _isHistorical {};
    // ushort _regIndex {};
    // uint _getIndex {};
    // uint _podDataOffset {};
    // uint _complexDataIndex {};

    //_isReadOnly
    // prop->isTemporary = (defaultTemporary || is_temporary);
    // prop->_isHistorical = (defaultNoHistory || is_no_history);
    // prop->checkMinValue = (min_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->checkMaxValue = (max_value != nullptr && (is_int_data_type || is_float_data_type));
    // prop->minValue = (min_value ? *min_value : 0);
    // prop->maxValue = (max_value ? *max_value : 0);

    const auto reg_index = static_cast<ushort>(_registeredProperties.size());

    // Disallow set or get accessors
    auto disable_get = false;
    auto disable_set = false;
    if (_isServer && IsEnumSet(prop->_accessType, Property::AccessType::ClientOnlyMask)) {
        disable_get = true;
        disable_set = true;
    }
    if (!_isServer && IsEnumSet(prop->_accessType, Property::AccessType::ServerOnlyMask)) {
        disable_get = true;
        disable_set = true;
    }
    if (!_isServer && (IsEnumSet(prop->_accessType, Property::AccessType::PublicMask) || IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) && !IsEnumSet(prop->_accessType, Property::AccessType::ModifiableMask)) {
        disable_set = true;
    }
    if (prop->_accessType == Property::AccessType::PublicStatic) {
        disable_set = true;
    }

    // PlainData property data offset
    auto data_base_offset = static_cast<uint>(-1);
    if (prop->_dataType == Property::DataType::PlainData && !disable_get && !IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
        const auto is_public = IsEnumSet(prop->_accessType, Property::AccessType::PublicMask);
        const auto is_protected = IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask);
        auto& space = (is_public ? _publicPodDataSpace : (is_protected ? _protectedPodDataSpace : _privatePodDataSpace));

        const auto space_size = static_cast<uint>(space.size());
        uint space_pos = 0;
        while (space_pos + prop->_baseSize <= space_size) {
            auto fail = false;
            for (uint i = 0; i < prop->_baseSize; i++) {
                if (space[space_pos + i]) {
                    fail = true;
                    break;
                }
            }
            if (!fail) {
                break;
            }
            space_pos += prop->_baseSize;
        }

        auto new_size = space_pos + prop->_baseSize;
        new_size += 8u - new_size % 8u;
        if (new_size > static_cast<uint>(space.size())) {
            space.resize(new_size);
        }

        for (uint i = 0; i < prop->_baseSize; i++) {
            space[space_pos + i] = true;
        }

        data_base_offset = space_pos;

        _wholePodDataSize = static_cast<uint>(_publicPodDataSpace.size() + _protectedPodDataSpace.size() + _privatePodDataSpace.size());
        RUNTIME_ASSERT((_wholePodDataSize % 8u) == 0u);
    }

    // Complex property data index
    auto complex_data_index = static_cast<uint>(-1);
    if (prop->_dataType != Property::DataType::PlainData && (!disable_get || !disable_set) && !IsEnumSet(prop->_accessType, Property::AccessType::VirtualMask)) {
        complex_data_index = _complexPropertiesCount++;
        if (IsEnumSet(prop->_accessType, Property::AccessType::PublicMask)) {
            _publicComplexDataProps.push_back(reg_index);
            _publicProtectedComplexDataProps.push_back(reg_index);
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            _protectedComplexDataProps.push_back(reg_index);
            _publicProtectedComplexDataProps.push_back(reg_index);
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::PrivateMask)) {
            _privateComplexDataProps.push_back(reg_index);
        }
        else {
            throw UnreachablePlaceException(LINE_STR);
        }
    }

    // Other flags
    prop->_regIndex = reg_index;
    prop->_complexDataIndex = complex_data_index;
    prop->_podDataOffset = data_base_offset;
    prop->_isReadable = !disable_get;
    prop->_isWritable = !disable_set;

    _registeredProperties.push_back(prop);
    _registeredPropertiesLookup.emplace(prop->GetName(), prop);

    // Fix plain data data offsets
    for (auto* prop : _registeredProperties) {
        if (prop->_podDataOffset == static_cast<uint>(-1)) {
            continue;
        }

        if (IsEnumSet(prop->_accessType, Property::AccessType::ProtectedMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size());
        }
        else if (IsEnumSet(prop->_accessType, Property::AccessType::PrivateMask)) {
            prop->_podDataOffset += static_cast<uint>(_publicPodDataSpace.size()) + static_cast<uint>(_protectedPodDataSpace.size());
        }
    }
}
