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

#include "AngelScriptString.h"
#include "AngelScriptArray.h"
#include "AngelScriptWrappedCall.h"

FO_BEGIN_NAMESPACE();

static auto StringFactory(AngelScript::asUINT length, const char* s) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return string(s, length);
}

static auto ConstructString(string* str) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    new (str) string();
}

static auto CopyConstructString(string* str, const string& other) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    new (str) string(other);
}

static auto DestructString(string* str) -> void
{
    FO_NO_STACK_TRACE_ENTRY();

    str->~string();
}

static auto AssignStringToString(string& str, const string& other) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    str = other;
    return str;
}

static auto AddAssignStringToString(string& str, const string& other) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    str += other;
    return str;
}

static auto AddStringToString(const string& str, const string& other) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return str + other;
}

static auto StringIsEmpty(const string& str) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return str.empty();
}

static auto AssignUInt64ToString(string& str, uint64 i) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    str = stream.str();
    return str;
}

static string& AddAssignUInt64ToString(string& str, uint64 i)
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    str += stream.str();
    return str;
}

static auto AddStringUInt64(const string& str, uint64 i) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    return str + stream.str();
}

static auto AddInt64String(const string& str, int64 i) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    return stream.str() + str;
}

static auto AssignInt64ToString(string& str, int64 i) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    str = stream.str();
    return str;
}

static auto AddAssignInt64ToString(string& str, int64 i) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    str += stream.str();
    return str;
}

static auto AddStringInt64(const string& str, int64 i) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    return str + stream.str();
}

static auto AddUInt64String(const string& str, uint64 i) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << i;
    return stream.str() + str;
}

static auto AssignDoubleToString(string& str, float64 f) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    str = stream.str();
    return str;
}

static auto AddAssignDoubleToString(string& str, float64 f) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    str += stream.str();
    return str;
}

static auto AssignFloatToString(string& str, float32 f) -> string&
{
    ostringstream stream;
    stream << f;
    str = stream.str();
    return str;
}

static auto AddAssignFloatToString(string& str, float32 f) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    str += stream.str();
    return str;
}

static auto AssignBoolToString(string& str, bool b) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << (b ? "true" : "false");
    str = stream.str();
    return str;
}

static auto AddAssignBoolToString(string& str, bool b) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << (b ? "true" : "false");
    str += stream.str();
    return str;
}

static auto AddStringDouble(const string& str, float64 f) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    return str + stream.str();
}

static auto AddDoubleString(const string& str, float64 f) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    return stream.str() + str;
}

static auto AddStringFloat(const string& str, float32 f) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    return str + stream.str();
}

static auto AddFloatString(const string& str, float32 f) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << f;
    return stream.str() + str;
}

static auto AddStringBool(const string& str, bool b) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << (b ? "true" : "false");
    return str + stream.str();
}

static auto AddBoolString(const string& str, bool b) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    ostringstream stream;
    stream << (b ? "true" : "false");
    return stream.str() + str;
}

static auto StringCmp(const string& str, const string& other) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (str < other) {
        return -1;
    }
    else if (str > other) {
        return 1;
    }

    return 0;
}

static auto StringEquals(const string& str, const string& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return str == other;
}

static auto IndexUtf8ToRaw(const string& str, int32& index, int32* length = nullptr, int32 offset = 0) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index < 0) {
        index = numeric_cast<int32>(strex(str).length_utf8()) + index;

        if (index < 0) {
            index = 0;

            if (length != nullptr) {
                if (!str.empty()) {
                    size_t decode_length = str.length();
                    utf8::Decode(str.c_str(), decode_length);
                    *length = numeric_cast<int32>(decode_length);
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
        size_t decode_length = str.length() - offset - numeric_cast<size_t>(s - begin);
        utf8::Decode(s, decode_length);

        if (index > 0) {
            s += decode_length;
            index--;
        }
        else {
            index = numeric_cast<int32>(s - begin);

            if (length != nullptr) {
                *length = numeric_cast<int32>(decode_length);
            }

            return true;
        }
    }

    index = numeric_cast<int32>(s - begin);

    if (length != nullptr) {
        *length = 0;
    }

    return false;
}

static auto IndexRawToUtf8(const string& str, int32 index) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    int32 result = 0;

    for (size_t i = 0; i < str.length() && index > 0;) {
        size_t decode_length = str.length() - i;
        utf8::Decode(&str[i], decode_length);
        i += decode_length;
        index -= numeric_cast<int32>(decode_length);
        result++;
    }

    return result;
}

static void ScriptString_Clear(string& str)
{
    FO_NO_STACK_TRACE_ENTRY();

    str.clear();
}

static auto ScriptString_Replace(const string& str, const string& str_from, const string& str_to) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex(str).replace(str_from, str_to);
}

static auto ScriptString_SubString(const string& str, int32 start, int32 count) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return "";
    }
    if (count >= 0) {
        IndexUtf8ToRaw(str, count, nullptr, start);
    }

    return str.substr(start, count >= 0 ? count : string::npos);
}

static int32 ScriptString_FindFirst(const string& str, const string& sub, int32 start)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.find(sub, start);
    return pos != string::npos ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_FindLast(const string& str, const string& sub, int32 start) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.rfind(sub);
    return pos != string::npos && numeric_cast<int32>(pos) >= start ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_FindFirstOf(const string& str, const string& chars, int32 start) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.find_first_of(chars, start);
    return pos != string::npos ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_FindFirstNotOf(const string& str, const string& chars, int32 start) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.find_first_not_of(chars, start);
    return pos != string::npos ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_FindLastOf(const string& str, const string& chars, int32 start) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.find_last_of(chars);
    return pos != string::npos && numeric_cast<int32>(pos) >= start ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_FindLastNotOf(const string& str, const string& chars, int32 start) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IndexUtf8ToRaw(str, start)) {
        return -1;
    }

    const auto pos = str.find_last_not_of(chars, start);
    return pos != string::npos && numeric_cast<int32>(pos) >= start ? IndexRawToUtf8(str, numeric_cast<int32>(pos)) : -1;
}

static auto ScriptString_GetAt(const string& str, int32 i) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    int32 length;

    if (!IndexUtf8ToRaw(str, i, &length)) {
        throw ScriptException("Out of range", i, length);
    }

    return string(str.c_str() + i, length);
}

static void ScriptString_SetAt(string& str, int32 i, string& value)
{
    FO_NO_STACK_TRACE_ENTRY();

    int32 length;

    if (!IndexUtf8ToRaw(str, i, &length)) {
        throw ScriptException("Out of range", i, length);
    }

    if (length != 0) {
        str.erase(i, length);
    }

    if (!value.empty()) {
        str.insert(i, value);
    }
}

static auto ScriptString_Length(const string& str) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(strex(str).length_utf8());
}

static auto ScriptString_RawLength(const string& str) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    return numeric_cast<int32>(str.length());
}

static void ScriptString_RawResize(string& str, int32 length)
{
    str.resize(numeric_cast<int32>(length));
}

static auto ScriptString_RawGet(const string& str, int32 index) -> uint8
{
    FO_NO_STACK_TRACE_ENTRY();

    return index >= 0 && index < numeric_cast<int32>(str.length()) ? str[index] : 0;
}

static void ScriptString_RawSet(string& str, int32 index, uint8 value)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (index >= 0 && index < numeric_cast<int32>(str.length())) {
        str[index] = std::bit_cast<char>(value);
    }
}

static auto ScriptString_ToInt(const string& str, int32 def_val) -> int32
{
    FO_NO_STACK_TRACE_ENTRY();

    char* end_str = nullptr;
    const auto result = numeric_cast<int32>(std::strtoll(str.c_str(), &end_str, 0));

    if (end_str == nullptr || end_str == str.c_str()) {
        return def_val;
    }

    return result;
}

static auto ScriptString_ToFloat(const string& str, float32 def_val) -> float32
{
    FO_NO_STACK_TRACE_ENTRY();

    char* end_str = nullptr;
    const auto result = numeric_cast<float32>(std::strtod(str.c_str(), &end_str));

    if (end_str == nullptr || end_str == str.c_str()) {
        return def_val;
    }

    return result;
}

static auto ScriptString_TryToInt(const string& str, int32& result) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    char* end_str = nullptr;
    const auto value = numeric_cast<int32>(std::strtoll(str.c_str(), &end_str, 0));

    if (end_str == nullptr || end_str == str.c_str()) {
        return false;
    }

    result = value;
    return true;
}

static auto ScriptString_TryToFloat(const string& str, float32& result) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    char* end_str = nullptr;
    const auto value = numeric_cast<float32>(std::strtod(str.c_str(), &end_str));

    if (end_str == nullptr || end_str == str.c_str()) {
        return false;
    }

    result = value;
    return true;
}

static auto ScriptString_StartsWith(const string& str, const string& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (str.length() < other.length()) {
        return false;
    }

    return str.starts_with(other);
}

static auto ScriptString_EndsWith(const string& str, const string& other) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (str.length() < other.length()) {
        return false;
    }

    return str.ends_with(other);
}

static auto ScriptString_Lower(const string& str) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex(str).lower_utf8();
}

static auto ScriptString_Upper(const string& str) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return strex(str).upper_utf8();
}

static auto ScriptString_Trim(const string& str, const string& chars) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t first = str.find_first_not_of(chars);

    if (first == string::npos) {
        return "";
    }

    const size_t last = str.find_last_not_of(chars);
    return str.substr(first, last - first + 1);
}

static auto ScriptString_TrimBegin(const string& str, const string& chars) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t first = str.find_first_not_of(chars);

    if (first == string::npos) {
        return "";
    }

    return str.substr(first);
}

static auto ScriptString_TrimEnd(const string& str, const string& chars) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    const size_t last = str.find_last_not_of(chars);
    if (last == string::npos) {
        return str;
    }
    return str.substr(0, last + 1);
}

static auto ScriptString_SplitExt(const string& str, const string& delim, bool remove_empty_entries) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);

    const auto* engine = ctx->GetEngine();
    auto* array = ScriptArray::Create(engine->GetTypeInfoById(engine->GetTypeIdByDecl("string[]")));

    size_t pos = 0;
    size_t prev = 0;
    int32 count = 0;

    while ((pos = str.find(delim, prev)) != string::npos) {
        if (pos - prev > 0 || !remove_empty_entries) {
            array->Resize(array->GetSize() + 1);
            static_cast<string*>(array->At(count))->assign(&str[prev], pos - prev);
            count++;
        }

        prev = pos + delim.length();
    }

    if (str.size() - prev > 0 || !remove_empty_entries) {
        array->Resize(array->GetSize() + 1);
        static_cast<string*>(array->At(count))->assign(&str[prev]);
    }

    return array;
}

static auto ScriptString_Split(const string& str, const string& delim) -> ScriptArray*
{
    FO_NO_STACK_TRACE_ENTRY();

    return ScriptString_SplitExt(str, delim, false);
}

static auto ScriptString_Join(const string& str, const ScriptArray* array) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (array == nullptr) {
        throw ScriptException("Array is null");
    }

    string result;

    if (array->GetSize() != 0) {
        const auto size = numeric_cast<int32>(array->GetSize());
        size_t capacity = size * str.size();

        for (int32 i = 0; i < size; i++) {
            const string& entry = *static_cast<const string*>(array->At(i));
            capacity += entry.length();
        }

        FO_RUNTIME_ASSERT(capacity < std::numeric_limits<int32>::max());
        result.reserve(capacity);

        for (int32 i = 0; i < size - 1; i++) {
            result += *static_cast<const string*>(array->At(i));
            result += str;
        }

        result += *static_cast<const string*>(array->At(size - 1));
    }

    return result;
}

static auto ScriptString_AssignAny(string& str, const any_t& other) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    str = other;
    return str;
}

static auto ScriptString_AddAssignAny(string& str, const any_t& other) -> string&
{
    FO_NO_STACK_TRACE_ENTRY();

    str += other;
    return str;
}

static auto ScriptString_AddAny(const string& str, const any_t& other) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return str + other;
}

static auto ScriptString_AddAnyR(const string& str, const any_t& other) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    return other + str;
}

void RegisterAngelScriptString(AngelScript::asIScriptEngine* engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 r = 0;

    r = engine->RegisterObjectType("string", sizeof(string), AngelScript::asOBJ_VALUE | AngelScript::asGetTypeTraits<string>());
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterStringFactory("string", SCRIPT_FUNC(StringFactory), SCRIPT_FUNC_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectBehaviour("string", AngelScript::asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS(ConstructString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("string", AngelScript::asBEHAVE_CONSTRUCT, "void f(const string &in)", SCRIPT_FUNC_THIS(CopyConstructString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectBehaviour("string", AngelScript::asBEHAVE_DESTRUCT, "void f()", SCRIPT_FUNC_THIS(DestructString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAssign(const string &in)", SCRIPT_FUNC_THIS(AssignStringToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(const string &in)", SCRIPT_FUNC_THIS(AddAssignStringToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "bool opEquals(const string &in) const", SCRIPT_FUNC_THIS(StringEquals), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int opCmp(const string &in) const", SCRIPT_FUNC_THIS(StringCmp), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(const string &in) const", SCRIPT_FUNC_THIS(AddStringToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "bool isEmpty() const", SCRIPT_FUNC_THIS(StringIsEmpty), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string &opAssign(double)", SCRIPT_FUNC_THIS(AssignDoubleToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(double)", SCRIPT_FUNC_THIS(AddAssignDoubleToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(double) const", SCRIPT_FUNC_THIS(AddStringDouble), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(double) const", SCRIPT_FUNC_THIS(AddDoubleString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string &opAssign(float)", SCRIPT_FUNC_THIS(AssignFloatToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(float)", SCRIPT_FUNC_THIS(AddAssignFloatToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(float) const", SCRIPT_FUNC_THIS(AddStringFloat), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(float) const", SCRIPT_FUNC_THIS(AddFloatString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string &opAssign(int64)", SCRIPT_FUNC_THIS(AssignInt64ToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(int64)", SCRIPT_FUNC_THIS(AddAssignInt64ToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(int64) const", SCRIPT_FUNC_THIS(AddStringInt64), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(int64) const", SCRIPT_FUNC_THIS(AddInt64String), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string &opAssign(uint64)", SCRIPT_FUNC_THIS(AssignUInt64ToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(uint64)", SCRIPT_FUNC_THIS(AddAssignUInt64ToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(uint64) const", SCRIPT_FUNC_THIS(AddStringUInt64), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(uint64) const", SCRIPT_FUNC_THIS(AddUInt64String), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string &opAssign(bool)", SCRIPT_FUNC_THIS(AssignBoolToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string &opAddAssign(bool)", SCRIPT_FUNC_THIS(AddAssignBoolToString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd(bool) const", SCRIPT_FUNC_THIS(AddStringBool), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string opAdd_r(bool) const", SCRIPT_FUNC_THIS(AddBoolString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "void clear()", SCRIPT_FUNC_THIS(ScriptString_Clear), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int length() const", SCRIPT_FUNC_THIS(ScriptString_Length), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int rawLength() const", SCRIPT_FUNC_THIS(ScriptString_RawLength), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawResize(int)", SCRIPT_FUNC_THIS(ScriptString_RawResize), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "uint8 rawGet(int) const", SCRIPT_FUNC_THIS(ScriptString_RawGet), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void rawSet(int, uint8)", SCRIPT_FUNC_THIS(ScriptString_RawSet), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string replace(const string &in, const string &in) const", SCRIPT_FUNC_THIS(ScriptString_Replace), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "string substr(int start = 0, int count = -1) const", SCRIPT_FUNC_THIS(ScriptString_SubString), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int find(const string &in, int start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirst), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstOf(const string &in, int start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findFirstNotOf(const string &in, int start = 0) const", SCRIPT_FUNC_THIS(ScriptString_FindFirstNotOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLast(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLast), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "int findLastNotOf(const string &in, int start = -1) const", SCRIPT_FUNC_THIS(ScriptString_FindLastNotOf), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "string get_opIndex(int) const", SCRIPT_FUNC_THIS(ScriptString_GetAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "void set_opIndex(int, const string &in)", SCRIPT_FUNC_THIS(ScriptString_SetAt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

    r = engine->RegisterObjectMethod("string", "int toInt(int defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToInt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "float toFloat(float defaultValue = 0) const", SCRIPT_FUNC_THIS(ScriptString_ToFloat), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToInt(int& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToInt), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
    r = engine->RegisterObjectMethod("string", "bool tryToFloat(float& result) const", SCRIPT_FUNC_THIS(ScriptString_TryToFloat), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);

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
    r = engine->RegisterObjectMethod("string", "string join(const array<string>@+) const", SCRIPT_FUNC_THIS(ScriptString_Join), SCRIPT_FUNC_THIS_CONV);
    FO_RUNTIME_ASSERT(r >= 0);
}

void RegisterAngelScriptStringAnyExtensions(AngelScript::asIScriptEngine* engine)
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

FO_END_NAMESPACE();
