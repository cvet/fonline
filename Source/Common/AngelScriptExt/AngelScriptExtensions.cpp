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

#include "AngelScriptExtensions.h"
#include "AngelScriptScriptDict.h"
#include "StringUtils.h"

#include "../autowrapper/aswrappedcall.h"
#include "scriptarray.h"

#ifdef AS_MAX_PORTABILITY
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#else
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#endif

static void CScriptArray_InsertFirst(CScriptArray* arr, void* value)
{
    arr->InsertAt(0, value);
}

static void CScriptArray_RemoveFirst(CScriptArray* arr)
{
    arr->RemoveAt(0);
}

static void CScriptArray_Grow(CScriptArray* arr, asUINT numElements)
{
    if (numElements == 0) {
        return;
    }

    arr->Resize(arr->GetSize() + numElements);
}

static void CScriptArray_Reduce(CScriptArray* arr, asUINT numElements)
{
    if (numElements == 0) {
        return;
    }

    asUINT size = arr->GetSize();
    if (numElements > size) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array size is less than reduce count");
        }
        return;
    }
    arr->Resize(size - numElements);
}

static void* CScriptArray_First(CScriptArray* arr)
{
    return arr->At(0);
}

static void* CScriptArray_Last(CScriptArray* arr)
{
    return arr->At(arr->GetSize() - 1);
}

static void CScriptArray_Clear(CScriptArray* arr)
{
    if (arr->GetSize() > 0) {
        arr->Resize(0);
    }
}

static bool CScriptArray_Exists(const CScriptArray* arr, void* value)
{
    return arr->Find(0, value) != -1;
}

static bool CScriptArray_Remove(CScriptArray* arr, void* value)
{
    int index = arr->Find(0, value);
    if (index != -1) {
        arr->RemoveAt(index);
        return true;
    }
    return false;
}

static uint CScriptArray_RemoveAll(CScriptArray* arr, void* value)
{
    uint count = 0;
    int index = 0;
    while (index < (int)arr->GetSize()) {
        index = arr->Find(index, value);
        if (index != -1) {
            arr->RemoveAt(index);
            count++;
        }
        else {
            break;
        }
    }
    return count;
}

static CScriptArray* CScriptArray_Factory(asITypeInfo* ti, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return nullptr;
    }

    CScriptArray* clone = CScriptArray::Create(ti);
    *clone = *other;
    return clone;
}

static CScriptArray* CScriptArray_Clone(const CScriptArray* arr)
{
    CScriptArray* clone = CScriptArray::Create(arr->GetArrayObjectType());
    *clone = *arr;
    return clone;
}

static void CScriptArray_Set(CScriptArray* arr, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return;
    }

    *arr = *other;
}

static void CScriptArray_InsertArrAt(CScriptArray* arr, uint index, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return;
    }

    arr->InsertAt(index, *other);
}

static void CScriptArray_InsertArrFirst(CScriptArray* arr, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return;
    }

    arr->InsertAt(0, *other);
}

static void CScriptArray_InsertArrLast(CScriptArray* arr, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return;
    }

    arr->InsertAt(arr->GetSize(), *other);
}

static bool CScriptArray_Equals(CScriptArray* arr, const CScriptArray* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return false;
    }

    return *arr == *other;
}

void ScriptExtensions::RegisterScriptArrayExtensions(asIScriptEngine* engine)
{
    int r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_InsertFirst), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeFirst()", SCRIPT_FUNC_THIS(CScriptArray_RemoveFirst), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void grow(uint)", SCRIPT_FUNC_THIS(CScriptArray_Grow), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void reduce(uint)", SCRIPT_FUNC_THIS(CScriptArray_Reduce), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& first()", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& first() const", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& last()", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& last() const", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void clear()", SCRIPT_FUNC_THIS(CScriptArray_Clear), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool exists(const T&in) const", SCRIPT_FUNC_THIS(CScriptArray_Exists), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool remove(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_Remove), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "uint removeAll(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_RemoveAll), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int& in, const array<T>@+)", SCRIPT_FUNC(CScriptArray_Factory), SCRIPT_FUNC_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "array<T>@ clone() const", SCRIPT_FUNC_THIS(CScriptArray_Clone), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void set(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_Set), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertAt(uint, const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrAt), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrFirst), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertLast(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrLast), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool equals(const array<T>@+) const", SCRIPT_FUNC_THIS(CScriptArray_Equals), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
}

static CScriptDict* ScriptDict_Factory(asITypeInfo* ti, const CScriptDict* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Dict is null");
        }
        return nullptr;
    }

    CScriptDict* clone = CScriptDict::Create(ti);
    *clone = *other;
    return clone;
}

static CScriptDict* ScriptDict_Clone(const CScriptDict* dict)
{
    CScriptDict* clone = CScriptDict::Create(dict->GetDictObjectType());
    *clone = *dict;
    return clone;
}

static bool ScriptDict_Equals(CScriptDict* dict, const CScriptDict* other)
{
    if (!other) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Dict is null");
        }
        return false;
    }

    return *dict == *other;
}

void ScriptExtensions::RegisterScriptDictExtensions(asIScriptEngine* engine)
{
    int r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int& in, const dict<T1,T2>@+)", SCRIPT_FUNC(ScriptDict_Factory), SCRIPT_FUNC_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>@ clone() const", SCRIPT_FUNC_THIS(ScriptDict_Clone), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool equals(const dict<T1,T2>@+) const", SCRIPT_FUNC_THIS(ScriptDict_Equals), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
}

static bool IndexUTF8ToRaw(const string& str, int& index, uint* length = nullptr, uint offset = 0)
{
    if (index < 0) {
        index = (int)_str(str).lengthUtf8() + index;
        if (index < 0) {
            index = 0;
            if (length) {
                if (!str.empty()) {
                    utf8::Decode(str.c_str(), length);
                }
                else {
                    *length = 0;
                }
            }
            return false;
        }
    }

    const char* begin = str.c_str() + offset;
    const char* s = begin;
    while (*s) {
        uint ch_length;
        utf8::Decode(s, &ch_length);
        if (index > 0) {
            s += ch_length;
            index--;
        }
        else {
            index = (uint)(s - begin);
            if (length) {
                *length = ch_length;
            }
            return true;
        }
    }
    index = (uint)(s - begin);
    if (length) {
        *length = 0;
    }
    return false;
}

static int IndexRawToUTF8(const string& str, int index)
{
    int result = 0;
    const char* s = str.c_str();
    while (index > 0 && *s) {
        uint ch_length;
        utf8::Decode(s, &ch_length);
        s += ch_length;
        index -= ch_length;
        result++;
    }
    return result;
}

static void ScriptString_Clear(string& str)
{
    str.clear();
}

static string ScriptString_SubString(const string& str, int start, int count)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return "";
    }
    if (count >= 0) {
        IndexUTF8ToRaw(str, count, nullptr, start);
    }
    return str.substr(start, count >= 0 ? count : std::string::npos);
}

static int ScriptString_FindFirst(const string& str, const string& sub, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.find(sub, start);
    return pos != -1 ? IndexRawToUTF8(str, pos) : -1;
}

static int ScriptString_FindLast(const string& str, const string& sub, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.rfind(sub);
    return pos != -1 && pos >= start ? IndexRawToUTF8(str, pos) : -1;
}

static int ScriptString_FindFirstOf(const string& str, const string& chars, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.find_first_of(chars, start);
    return pos != -1 ? IndexRawToUTF8(str, pos) : -1;
}

static int ScriptString_FindFirstNotOf(const string& str, const string& chars, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.find_first_not_of(chars, start);
    return pos != -1 ? IndexRawToUTF8(str, pos) : -1;
}

static int ScriptString_FindLastOf(const string& str, const string& chars, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.find_last_of(chars);
    return pos != -1 && pos >= start ? IndexRawToUTF8(str, pos) : -1;
}

static int ScriptString_FindLastNotOf(const string& str, const string& chars, int start)
{
    if (!IndexUTF8ToRaw(str, start)) {
        return -1;
    }
    int pos = (int)str.find_last_not_of(chars, start);
    return pos != -1 && pos >= start ? IndexRawToUTF8(str, pos) : -1;
}

static string ScriptString_GetAt(const string& str, int i)
{
    uint length;
    if (!IndexUTF8ToRaw(str, i, &length)) {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return {};
    }

    return string(str.c_str() + i, length);
}

static void ScriptString_SetAt(string& str, int i, string& value)
{
    uint length;
    if (!IndexUTF8ToRaw(str, i, &length)) {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return;
    }

    if (length) {
        str.erase(i, length);
    }
    if (value.length()) {
        str.insert(i, value.c_str());
    }
}

static uint ScriptString_Length(const string& str)
{
    return _str(str).lengthUtf8();
}

static uint ScriptString_RawLength(const string& str)
{
    return (uint)str.length();
}

static void ScriptString_RawResize(string& str, uint length)
{
    str.resize(length);
}

static uchar ScriptString_RawGet(const string& str, uint index)
{
    return index < (uint)str.length() ? str[index] : 0;
}

static void ScriptString_RawSet(string& str, uint index, uchar value)
{
    if (index < (uint)str.length()) {
        str[index] = (char)value;
    }
}

static int ScriptString_ToInt(const string& str, int defaultValue)
{
    const char* p = str.c_str();
    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    char* end_str = nullptr;
    int result;
    if (p[0] && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        result = (int)strtol(p + 2, &end_str, 16);
    }
    else {
        result = (int)strtol(p, &end_str, 10);
    }

    if (!end_str || end_str == p) {
        return defaultValue;
    }

    while (*end_str == ' ' || *end_str == '\t') {
        ++end_str;
    }
    if (*end_str) {
        return defaultValue;
    }

    return result;
}

static float ScriptString_ToFloat(const string& str, float defaultValue)
{
    const char* p = str.c_str();
    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    char* end_str = NULL;
    float result = (float)strtod(p, &end_str);

    if (!end_str || end_str == p) {
        return defaultValue;
    }

    while (*end_str == ' ' || *end_str == '\t') {
        ++end_str;
    }
    if (*end_str) {
        return defaultValue;
    }

    return result;
}

static bool ScriptString_TryToInt(const string& str, int& result)
{
    const char* p = str.c_str();
    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    char* end_str = nullptr;
    int result_;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        result_ = (int)strtol(p + 2, &end_str, 16);
    }
    else {
        result_ = (int)strtol(p, &end_str, 10);
    }

    if (!end_str || end_str == p) {
        return false;
    }

    while (*end_str == ' ' || *end_str == '\t') {
        ++end_str;
    }
    if (*end_str) {
        return false;
    }

    result = result_;
    return true;
}

static bool ScriptString_TryToFloat(const string& str, float& result)
{
    const char* p = str.c_str();
    while (*p == ' ' || *p == '\t') {
        ++p;
    }

    char* end_str = NULL;
    float result_ = (float)strtod(p, &end_str);

    if (!end_str || end_str == p) {
        return false;
    }

    while (*end_str == ' ' || *end_str == '\t') {
        ++end_str;
    }
    if (*end_str) {
        return false;
    }

    result = result_;
    return true;
}

static bool ScriptString_StartsWith(const string& str, const string& other)
{
    if (str.length() < other.length()) {
        return false;
    }
    return str.compare(0, other.length(), other) == 0;
}

static bool ScriptString_EndsWith(const string& str, const string& other)
{
    if (str.length() < other.length()) {
        return false;
    }
    return str.compare(str.length() - other.length(), other.length(), other) == 0;
}

static string ScriptString_Lower(const string& str)
{
    return _str(str).lowerUtf8();
}

static string ScriptString_Upper(const string& str)
{
    return _str(str).upperUtf8();
}

static string ScriptString_Trim(const string& str, const string& chars)
{
    size_t first = str.find_first_not_of(chars);
    if (first == string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(chars);
    return str.substr(first, (last - first + 1));
}

static string ScriptString_TrimBegin(const string& str, const string& chars)
{
    size_t first = str.find_first_not_of(chars);
    if (first == string::npos) {
        return "";
    }
    return str.substr(first);
}

static string ScriptString_TrimEnd(const string& str, const string& chars)
{
    size_t last = str.find_last_not_of(chars);
    if (last == string::npos) {
        return str;
    }
    return str.substr(0, last + 1);
}

static CScriptArray* ScriptString_Split(const string& str, const string& delim)
{
    asIScriptEngine* engine = asGetActiveContext()->GetEngine();
    CScriptArray* array = CScriptArray::Create(engine->GetTypeInfoById(engine->GetTypeIdByDecl("string[]")));

    // Find the existence of the delimiter in the input string
    int pos = 0, prev = 0, count = 0;
    while ((pos = (int)str.find(delim, prev)) != (int)string::npos) {
        // Add the part to the array
        array->Resize(array->GetSize() + 1);
        ((string*)array->At(count))->assign(&str[prev], pos - prev);

        // Find the next part
        count++;
        prev = pos + (int)delim.length();
    }

    // Add the remaining part
    array->Resize(array->GetSize() + 1);
    ((string*)array->At(count))->assign(&str[prev]);

    return array;
}

static string ScriptString_Join(const CScriptArray* array, const string& delim)
{
    if (!array) {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx) {
            ctx->SetException("Array is null");
        }
        return "";
    }

    // Create the new string
    string str = "";
    if (array->GetSize()) {
        int n;
        for (n = 0; n < (int)array->GetSize() - 1; n++) {
            str += *(string*)array->At(n);
            str += delim;
        }

        // Add the last part
        str += *(string*)array->At(n);
    }

    return str;
}

void ScriptExtensions::RegisterScriptStdStringExtensions(asIScriptEngine* engine)
{
    int r = engine->RegisterObjectMethod("string", "void clear()", SCRIPT_FUNC_THIS(ScriptString_Clear), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint length() const", SCRIPT_FUNC_THIS(ScriptString_Length), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint rawLength() const", SCRIPT_FUNC_THIS(ScriptString_RawLength), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawResize(uint)", SCRIPT_FUNC_THIS(ScriptString_RawResize), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint8 rawGet(uint) const", SCRIPT_FUNC_THIS(ScriptString_RawGet), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawSet(uint, uint8)", SCRIPT_FUNC_THIS(ScriptString_RawSet), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string substr(uint start = 0, int count = -1) const", SCRIPT_FUNC_THIS(ScriptString_SubString), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int find(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirst), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstOf(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstOf), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstNotOf(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstNotOf), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLast(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLast), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastOf), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastNotOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastNotOf), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    // Register the index operator, both as a mutator and as an inspector
    r = engine->RegisterObjectMethod("string", "string get_opIndex(int) const", SCRIPT_FUNC_THIS(ScriptString_GetAt), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void set_opIndex(int, const string &in)", SCRIPT_FUNC_THIS(ScriptString_SetAt), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    // Conversion methods
    r = engine->RegisterObjectMethod("string", "int toInt(int defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToInt), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "float toFloat(float defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToFloat), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToInt(int& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToInt), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToFloat(float& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToFloat), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    // Find methods
    r = engine->RegisterObjectMethod("string", "bool startsWith(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_StartsWith), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool endsWith(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_EndsWith), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string lower() const", SCRIPT_FUNC_THIS(ScriptString_Lower), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string upper() const", SCRIPT_FUNC_THIS(ScriptString_Upper), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string trim(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_Trim), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string trimBegin(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_TrimBegin), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string trimEnd(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_TrimEnd), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "array<string>@ split(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_Split), SCRIPT_FUNC_THIS_CONV);
    RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterGlobalFunction("string join(const array<string>@+, const string &in)", SCRIPT_FUNC(ScriptString_Join), SCRIPT_FUNC_CONV);
    RUNTIME_ASSERT(r >= 0);
}
