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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_USING_NAMESPACE();

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

    const asUINT size = arr->GetSize();

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
    const int32 index = arr->Find(0, value);

    if (index != -1) {
        arr->RemoveAt(index);
        return true;
    }

    return false;
}

static uint32 CScriptArray_RemoveAll(CScriptArray* arr, void* value)
{
    uint32 count = 0;
    int32 index = 0;

    while (index < static_cast<int32>(arr->GetSize())) {
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

static void CScriptArray_InsertArrAt(CScriptArray* arr, uint32 index, const CScriptArray* other)
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
    int32 r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_InsertFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void removeFirst()", SCRIPT_FUNC_THIS(CScriptArray_RemoveFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void grow(uint)", SCRIPT_FUNC_THIS(CScriptArray_Grow), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void reduce(uint)", SCRIPT_FUNC_THIS(CScriptArray_Reduce), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& first()", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& first() const", SCRIPT_FUNC_THIS(CScriptArray_First), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& last()", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "const T& last() const", SCRIPT_FUNC_THIS(CScriptArray_Last), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void clear()", SCRIPT_FUNC_THIS(CScriptArray_Clear), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool exists(const T&in) const", SCRIPT_FUNC_THIS(CScriptArray_Exists), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool remove(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_Remove), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "uint removeAll(const T&in)", SCRIPT_FUNC_THIS(CScriptArray_RemoveAll), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int& in, const array<T>@+)", SCRIPT_FUNC(CScriptArray_Factory), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "array<T>@ clone() const", SCRIPT_FUNC_THIS(CScriptArray_Clone), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void set(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_Set), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertAt(uint, const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "void insertLast(const array<T>@+)", SCRIPT_FUNC_THIS(CScriptArray_InsertArrLast), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool equals(const array<T>@+) const", SCRIPT_FUNC_THIS(CScriptArray_Equals), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
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
    int32 r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int& in, const dict<T1,T2>@+)", SCRIPT_FUNC(ScriptDict_Factory), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>@ clone() const", SCRIPT_FUNC_THIS(ScriptDict_Clone), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "bool equals(const dict<T1,T2>@+) const", SCRIPT_FUNC_THIS(ScriptDict_Equals), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
}

static bool IndexUtf8ToRaw(const string& str, int32& index, uint32* length = nullptr, uint32 offset = 0)
{
    if (index < 0) {
        index = static_cast<int32>(strex(str).lengthUtf8()) + index;

        if (index < 0) {
            index = 0;

            if (length != nullptr) {
                if (!str.empty()) {
                    size_t decode_length = str.length();
                    utf8::Decode(str.c_str(), decode_length);
                    *length = static_cast<uint32>(decode_length);
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

    while (*s != 0) {
        size_t decode_length = str.length() - offset - static_cast<size_t>(s - begin);
        utf8::Decode(s, decode_length);

        if (index > 0) {
            s += decode_length;
            index--;
        }
        else {
            index = static_cast<int32>(s - begin);

            if (length != nullptr) {
                *length = static_cast<uint32>(decode_length);
            }

            return true;
        }
    }

    index = static_cast<int32>(s - begin);

    if (length != nullptr) {
        *length = 0;
    }

    return false;
}

static int32 IndexRawToUtf8(const string& str, int32 index)
{
    int32 result = 0;

    for (size_t i = 0; i < str.length() && index > 0;) {
        size_t decode_length = str.length() - i;
        utf8::Decode(&str[i], decode_length);
        i += decode_length;
        index -= static_cast<int32>(decode_length);
        result++;
    }

    return result;
}

static void ScriptString_Clear(string& str)
{
    str.clear();
}

static string ScriptString_Replace(const string& str, const string& str_from, const string& str_to)
{
    return strex(str).replace(str_from, str_to);
}

static string ScriptString_SubString(const string& str, int32 start, int32 count)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return "";
    }
    if (count >= 0) {
        IndexUtf8ToRaw(str, count, nullptr, start);
    }

    return str.substr(start, count >= 0 ? count : std::string::npos);
}

static int32 ScriptString_FindFirst(const string& str, const string& sub, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.find(sub, start));
    return pos != -1 ? IndexRawToUtf8(str, pos) : -1;
}

static int32 ScriptString_FindLast(const string& str, const string& sub, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.rfind(sub));
    return pos != -1 && pos >= start ? IndexRawToUtf8(str, pos) : -1;
}

static int32 ScriptString_FindFirstOf(const string& str, const string& chars, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.find_first_of(chars, start));
    return pos != -1 ? IndexRawToUtf8(str, pos) : -1;
}

static int32 ScriptString_FindFirstNotOf(const string& str, const string& chars, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.find_first_not_of(chars, start));
    return pos != -1 ? IndexRawToUtf8(str, pos) : -1;
}

static int32 ScriptString_FindLastOf(const string& str, const string& chars, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.find_last_of(chars));
    return pos != -1 && pos >= start ? IndexRawToUtf8(str, pos) : -1;
}

static int32 ScriptString_FindLastNotOf(const string& str, const string& chars, int32 start)
{
    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const int32 pos = static_cast<int32>(str.find_last_not_of(chars, start));
    return pos != -1 && pos >= start ? IndexRawToUtf8(str, pos) : -1;
}

static string ScriptString_GetAt(const string& str, int32 i)
{
    uint32 length;

    if (!IndexUtf8ToRaw(str, i, &length)) {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException("Out of range");
        return {};
    }

    return string(str.c_str() + i, length);
}

static void ScriptString_SetAt(string& str, int32 i, string& value)
{
    uint32 length;

    if (!IndexUtf8ToRaw(str, i, &length)) {
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

static uint32 ScriptString_Length(const string& str)
{
    return strex(str).lengthUtf8();
}

static uint32 ScriptString_RawLength(const string& str)
{
    return static_cast<uint32>(str.length());
}

static void ScriptString_RawResize(string& str, uint32 length)
{
    str.resize(length);
}

static uint8 ScriptString_RawGet(const string& str, uint32 index)
{
    return index < static_cast<uint32>(str.length()) ? str[index] : 0;
}

static void ScriptString_RawSet(string& str, uint32 index, uint8 value)
{
    if (index < static_cast<uint32>(str.length())) {
        str[index] = static_cast<char>(value);
    }
}

static int32 ScriptString_ToInt(const string& str, int32 defaultValue)
{
    char* end_str = nullptr;
    const auto result = static_cast<int32>(std::strtoll(str.c_str(), &end_str, 0));

    if (end_str == nullptr || end_str == str.c_str()) {
        return defaultValue;
    }

    return result;
}

static float32 ScriptString_ToFloat(const string& str, float32 defaultValue)
{
    char* end_str = nullptr;
    const auto result = static_cast<float32>(std::strtod(str.c_str(), &end_str));

    if (end_str == nullptr || end_str == str.c_str()) {
        return defaultValue;
    }

    return result;
}

static bool ScriptString_TryToInt(const string& str, int32& result)
{
    char* end_str = nullptr;
    const auto result_ = static_cast<int32>(std::strtoll(str.c_str(), &end_str, 0));

    if (end_str == nullptr || end_str == str.c_str()) {
        return false;
    }

    result = result_;
    return true;
}

static bool ScriptString_TryToFloat(const string& str, float32& result)
{
    char* end_str = nullptr;
    const auto result_ = static_cast<float32>(std::strtod(str.c_str(), &end_str));

    if (end_str == nullptr || end_str == str.c_str()) {
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
    return strex(str).lowerUtf8();
}

static string ScriptString_Upper(const string& str)
{
    return strex(str).upperUtf8();
}

static string ScriptString_Trim(const string& str, const string& chars)
{
    const size_t first = str.find_first_not_of(chars);
    if (first == string::npos) {
        return "";
    }
    const size_t last = str.find_last_not_of(chars);
    return str.substr(first, (last - first + 1));
}

static string ScriptString_TrimBegin(const string& str, const string& chars)
{
    const size_t first = str.find_first_not_of(chars);
    if (first == string::npos) {
        return "";
    }
    return str.substr(first);
}

static string ScriptString_TrimEnd(const string& str, const string& chars)
{
    const size_t last = str.find_last_not_of(chars);
    if (last == string::npos) {
        return str;
    }
    return str.substr(0, last + 1);
}

static CScriptArray* ScriptString_SplitExt(const string& str, const string& delim, bool remove_empty_entries)
{
    const asIScriptEngine* engine = asGetActiveContext()->GetEngine();
    CScriptArray* array = CScriptArray::Create(engine->GetTypeInfoById(engine->GetTypeIdByDecl("string[]")));

    // Find the existence of the delimiter in the input string
    int32 pos = 0, prev = 0, count = 0;

    while ((pos = static_cast<int32>(str.find(delim, prev))) != static_cast<int32>(string::npos)) {
        // Add the part to the array
        if (pos - prev > 0 || !remove_empty_entries) {
            array->Resize(array->GetSize() + 1);
            static_cast<string*>(array->At(count))->assign(&str[prev], pos - prev);

            count++;
        }

        // Find the next part
        prev = pos + static_cast<int32>(delim.length());
    }

    // Add the remaining part
    if (str.size() - prev > 0 || !remove_empty_entries) {
        array->Resize(array->GetSize() + 1);
        static_cast<string*>(array->At(count))->assign(&str[prev]);
    }

    return array;
}

static CScriptArray* ScriptString_Split(const string& str, const string& delim)
{
    return ScriptString_SplitExt(str, delim, false);
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
        int32 n;

        for (n = 0; n < static_cast<int32>(array->GetSize()) - 1; n++) {
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
    int32 r = engine->RegisterObjectMethod("string", "void clear()", SCRIPT_FUNC_THIS(ScriptString_Clear), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint length() const", SCRIPT_FUNC_THIS(ScriptString_Length), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint rawLength() const", SCRIPT_FUNC_THIS(ScriptString_RawLength), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawResize(uint)", SCRIPT_FUNC_THIS(ScriptString_RawResize), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint8 rawGet(uint) const", SCRIPT_FUNC_THIS(ScriptString_RawGet), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawSet(uint, uint8)", SCRIPT_FUNC_THIS(ScriptString_RawSet), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string replace(const string &in, const string &in) const", SCRIPT_FUNC_THIS(ScriptString_Replace), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string substr(uint start = 0, int count = -1) const", SCRIPT_FUNC_THIS(ScriptString_SubString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int find(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstOf(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstNotOf(const string &in, uint start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstNotOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLast(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLast), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastNotOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastNotOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    // Register the index operator, both as a mutator and as an inspector
    r = engine->RegisterObjectMethod("string", "string get_opIndex(int) const", SCRIPT_FUNC_THIS(ScriptString_GetAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void set_opIndex(int, const string &in)", SCRIPT_FUNC_THIS(ScriptString_SetAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    // Conversion methods
    r = engine->RegisterObjectMethod("string", "int toInt(int defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToInt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "float toFloat(float defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToFloat), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToInt(int& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToInt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToFloat(float& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToFloat), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    // Find methods
    r = engine->RegisterObjectMethod("string", "bool startsWith(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_StartsWith), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool endsWith(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_EndsWith), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string lower() const", SCRIPT_FUNC_THIS(ScriptString_Lower), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string upper() const", SCRIPT_FUNC_THIS(ScriptString_Upper), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string trim(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_Trim), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string trimBegin(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_TrimBegin), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string trimEnd(const string &in chars = \" \") const", SCRIPT_FUNC_THIS(ScriptString_TrimEnd), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "array<string>@ split(const string &in) const", SCRIPT_FUNC_THIS(ScriptString_Split), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "array<string>@ split(const string &in, bool removeEmptyEntries) const", SCRIPT_FUNC_THIS(ScriptString_SplitExt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterGlobalFunction("string join(const array<string>@+, const string &in)", SCRIPT_FUNC(ScriptString_Join), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
}

static string& ScriptString_AssignAny(string& str, const any_t& other)
{
    str = other;
    return str;
}

static string& ScriptString_AddAssignAny(string& str, const any_t& other)
{
    str += other;
    return str;
}

static string ScriptString_AddAny(const string& str, const any_t& other)
{
    return str + other;
}

static string ScriptString_AddAnyR(const string& str, const any_t& other)
{
    return other + str;
}

void ScriptExtensions::RegisterScriptStdStringAnyExtensions(asIScriptEngine* engine)
{
    int32 r = engine->RegisterObjectMethod("string", "string& opAssign(const any &in)", SCRIPT_FUNC_THIS(ScriptString_AssignAny), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string& opAddAssign(const any &in)", SCRIPT_FUNC_THIS(ScriptString_AddAssignAny), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(const any &in) const", SCRIPT_FUNC_THIS(ScriptString_AddAny), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(const any &in) const", SCRIPT_FUNC_THIS(ScriptString_AddAnyR), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
}
