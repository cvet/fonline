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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AngelScriptHelpers.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptDict.h"
#include "EngineBase.h"
#include "EntityProtos.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

constexpr AngelScript::asPWORD AS_TYPE_INFO_CACHE_USER_DATA = 1100;
constexpr AngelScript::asPWORD AS_TYPE_FAST_COMPARE_USER_DATA = 1101;

struct ScriptTypeInfoCache
{
    std::mutex Locker {};
    unordered_map<string, AngelScript::asITypeInfo*> Map {};
    unordered_map<const Property*, AngelScript::asITypeInfo*> ByProperty {};
};

void SetScriptTypeFastCompare(AngelScript::asITypeInfo* type, ScriptFastCompareFunc func)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(type);
    type->SetUserData(reinterpret_cast<void*>(func), AS_TYPE_FAST_COMPARE_USER_DATA);
}

auto GetScriptTypeFastCompare(const AngelScript::asITypeInfo* type) -> ScriptFastCompareFunc
{
    FO_NO_STACK_TRACE_ENTRY();

    return reinterpret_cast<ScriptFastCompareFunc>(type->GetUserData(AS_TYPE_FAST_COMPARE_USER_DATA));
}

[[noreturn]] void ThrowScriptCoreException(string_view file, int32_t line, int32_t result)
{
    FO_NO_STACK_TRACE_ENTRY();

    const string_view file_name = strvex(file).extract_file_name().erase_file_extension();
    throw ScriptCoreException(strex("File: {}", file_name), strex("Line: {}", line), strex("Result: {}", result));
}

auto GetScriptBackend(BaseEngine* engine) -> AngelScriptBackend*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* backend = engine->GetBackend<AngelScriptBackend>(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX);
    FO_RUNTIME_ASSERT(backend);
    return backend;
}

auto GetScriptBackend(AngelScript::asIScriptEngine* as_engine) -> AngelScriptBackend*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());
    FO_RUNTIME_ASSERT(backend);
    return backend;
}

auto GetEngineMetadata(AngelScript::asIScriptEngine* as_engine) -> const EngineMetadata*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());
    FO_RUNTIME_ASSERT(backend);
    return backend->GetMetadata();
}

auto GetGameEngine(AngelScript::asIScriptEngine* as_engine) -> BaseEngine*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());
    FO_RUNTIME_ASSERT(backend);
    return backend->GetGameEngine();
}

void CheckScriptEntityNonNull(const Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity == nullptr) {
        throw ScriptException("Access to null entity");
    }
}

void CheckScriptEntityNonDestroyed(const Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity != nullptr && entity->IsDestroyed()) {
        throw ScriptException("Access to destroyed entity");
    }
}

void CheckScriptEntityAccessAndNonDestroyed(const Entity* entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity != nullptr) {
        entity->ValidateAccess();

        if (entity->IsDestroyed()) {
            throw ScriptException("Access to destroyed entity");
        }
    }
}

auto MakeScriptTypeName(const BaseTypeDesc& type) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (type.Name == "int32") {
        return string("int");
    }
    else if (type.Name == "uint32") {
        return string("uint");
    }
    else if (type.Name == "float32") {
        return string("float");
    }
    else if (type.Name == "float64") {
        return string("double");
    }

    if (type.IsSingleton) {
        return strex("{}Singleton", type.Name);
    }
    else {
        return type.Name;
    }
}

auto MakeScriptTypeName(const ComplexTypeDesc& type) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(type);

    string result;

    if (type.Kind == ComplexTypeKind::DictOfArray) {
        FO_RUNTIME_ASSERT(type.KeyType);
        result = strex("dict<{},array<{}>>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        FO_RUNTIME_ASSERT(type.KeyType);
        result = strex("dict<{},{}>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Array) {
        result = strex("array<{}>", MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Callback) {
        result = "callback";

        for (size_t i = 0; i < type.CallbackArgs->size(); i++) {
            const auto& cb_arg = (*type.CallbackArgs)[i];
            result += "_";

            if (cb_arg.Kind == ComplexTypeKind::None) {
                result += "void";
            }
            else if (cb_arg.Kind == ComplexTypeKind::DictOfArray) {
                FO_RUNTIME_ASSERT(cb_arg.KeyType);
                result += strex("{}_to_{}_arr", cb_arg.KeyType.value().Name, cb_arg.BaseType.Name);
            }
            else if (cb_arg.Kind == ComplexTypeKind::Dict) {
                FO_RUNTIME_ASSERT(cb_arg.KeyType);
                result += strex("{}_to_{}", cb_arg.KeyType.value().Name, cb_arg.BaseType.Name);
            }
            else if (cb_arg.Kind == ComplexTypeKind::Array) {
                result += strex("{}_arr", cb_arg.BaseType.Name);
            }
            else {
                result += cb_arg.BaseType.Name;
            }
        }
    }
    else {
        result = MakeScriptTypeName(type.BaseType);
    }

    return result;
}

auto MakeScriptArgName(const ComplexTypeDesc& type, bool nullable) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(type);

    string result = MakeScriptTypeName(type);

    if (type.Kind == ComplexTypeKind::Simple && !(type.BaseType.IsEntity || type.BaseType.IsRefType)) {
        if (type.IsMutable) {
            result += "&";
        }
    }
    else if (type.Kind == ComplexTypeKind::Simple && type.BaseType.IsGlobalEntity) {
        result += "@";

        if (nullable) {
            result += "?";
        }
    }
    else {
        result += "@";

        if (nullable) {
            result += "?";
        }

        if (type.IsMutable) {
            result += "&";
        }
        else {
            result += "+";
        }
    }

    return result;
}

auto MakeScriptArgsName(const_span<ArgDesc> args) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;
    result.reserve(128);

    for (size_t i = 0; i < args.size(); i++) {
        const auto& arg = args[i];

        if (i > 0) {
            result += ", ";
        }

        result += MakeScriptArgName(arg.Type, arg.Nullable);
        result += " ";
        result += arg.Name;

        if (!arg.DefaultValue.empty()) {
            result += " = ";
            result += arg.DefaultValue;
        }
    }

    return result;
}

auto MakeScriptReturnName(const ComplexTypeDesc& type, bool pass_ownership, bool nullable) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type) {
        return "void";
    }

    string result = MakeScriptArgName(type, nullable);

    if (type.Kind == ComplexTypeKind::Simple) {
        if ((type.BaseType.IsEntity && !type.BaseType.IsGlobalEntity) || type.BaseType.IsRefType) {
            FO_RUNTIME_ASSERT(result.back() == '+');

            if (pass_ownership) {
                result.pop_back();
            }
        }
    }
    else {
        FO_RUNTIME_ASSERT(result.back() == '+');
        result.pop_back();
    }

    return result;
}

auto MakeScriptPropertyName(const Property* prop) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result;

    if (prop->IsDict()) {
        if (prop->IsDictOfArray()) {
            result = strex("dict<{},array<{}>>", MakeScriptTypeName(prop->GetDictKeyType()), MakeScriptTypeName(prop->GetBaseType()));
        }
        else {
            result = strex("dict<{},{}>", MakeScriptTypeName(prop->GetDictKeyType()), MakeScriptTypeName(prop->GetBaseType()));
        }
    }
    else if (prop->IsArray()) {
        result = strex("array<{}>", MakeScriptTypeName(prop->GetBaseType()));
    }
    else {
        result = MakeScriptTypeName(prop->GetBaseType());
    }

    return result;
}

// Normalize script type declarations to the canonical form used by MakeScriptPropertyName
// Example: "dict<string, Critter@[]@>@" becomes "dict<string,array<Critter@>>"
auto NormalizeScriptPropertyDecl(string_view decl) -> string
{
    FO_STACK_TRACE_ENTRY();

    string fixed_decl = strex(decl).replace("[]@", "[]").str();

    while (true) {
        const auto array_pos = fixed_decl.find("[]");

        if (array_pos == string::npos) {
            break;
        }

        size_t element_begin = array_pos;

        if (element_begin > 0 && fixed_decl[element_begin - 1] == '>') {
            int32_t nested_level = 1;
            element_begin--;

            while (element_begin > 0) {
                element_begin--;

                if (fixed_decl[element_begin] == '>') {
                    nested_level++;
                }
                else if (fixed_decl[element_begin] == '<') {
                    nested_level--;

                    if (nested_level == 0) {
                        while (element_begin > 0 && (std::isalnum(static_cast<unsigned char>(fixed_decl[element_begin - 1])) != 0 || fixed_decl[element_begin - 1] == '_' || fixed_decl[element_begin - 1] == ':' || fixed_decl[element_begin - 1] == '@')) {
                            element_begin--;
                        }
                        break;
                    }
                }
            }
        }
        else {
            while (element_begin > 0 && (std::isalnum(static_cast<unsigned char>(fixed_decl[element_begin - 1])) != 0 || fixed_decl[element_begin - 1] == '_' || fixed_decl[element_begin - 1] == ':' || fixed_decl[element_begin - 1] == '@')) {
                element_begin--;
            }
        }

        const auto element_decl = fixed_decl.substr(element_begin, array_pos - element_begin);
        fixed_decl = strex("{}array<{}>{}", fixed_decl.substr(0, element_begin), element_decl, fixed_decl.substr(array_pos + 2)).str();
    }

    if ((fixed_decl.starts_with("array<") || fixed_decl.starts_with("dict<")) && !fixed_decl.empty() && fixed_decl.back() == '@') {
        fixed_decl.pop_back();
    }

    return fixed_decl;
}

static void CleanupTypeInfoCache(AngelScript::asIScriptEngine* engine) noexcept
{
    FO_STACK_TRACE_ENTRY();

    delete cast_from_void<ScriptTypeInfoCache*>(engine->GetUserData(AS_TYPE_INFO_CACHE_USER_DATA));
    engine->SetUserData(nullptr, AS_TYPE_INFO_CACHE_USER_DATA);
}

static auto GetTypeInfoCache(AngelScript::asIScriptEngine* as_engine) -> ScriptTypeInfoCache*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* cache = cast_from_void<ScriptTypeInfoCache*>(as_engine->GetUserData(AS_TYPE_INFO_CACHE_USER_DATA));

    if (cache == nullptr) {
        cache = new ScriptTypeInfoCache();
        as_engine->SetUserData(cast_to_void(cache), AS_TYPE_INFO_CACHE_USER_DATA);
        as_engine->SetEngineUserDataCleanupCallback(CleanupTypeInfoCache, AS_TYPE_INFO_CACHE_USER_DATA);
    }

    return cache;
}

static auto LookupCachedTypeInfo(AngelScript::asIScriptEngine* as_engine, const char* type) -> AngelScript::asITypeInfo*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* cache = GetTypeInfoCache(as_engine);
    std::scoped_lock lock {cache->Locker};

    if (const auto it = cache->Map.find(type); it != cache->Map.end()) {
        return it->second;
    }

    const auto type_id = as_engine->GetTypeIdByDecl(type);
    FO_RUNTIME_ASSERT(type_id);
    auto* info = as_engine->GetTypeInfoById(type_id);
    FO_RUNTIME_ASSERT(info);
    cache->Map.emplace(type, info);
    return info;
}

static auto LookupCachedTypeInfoForProperty(AngelScript::asIScriptEngine* as_engine, const Property* prop) -> AngelScript::asITypeInfo*
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* cache = GetTypeInfoCache(as_engine);
    std::scoped_lock lock {cache->Locker};

    if (const auto it = cache->ByProperty.find(prop); it != cache->ByProperty.end()) {
        return it->second;
    }

    const auto type_name = MakeScriptPropertyName(prop);
    const auto type_id = as_engine->GetTypeIdByDecl(type_name.c_str());
    FO_RUNTIME_ASSERT(type_id);
    auto* info = as_engine->GetTypeInfoById(type_id);
    FO_RUNTIME_ASSERT(info);
    cache->ByProperty.emplace(prop, info);
    cache->Map.try_emplace(type_name, info);
    return info;
}

auto CreateScriptArray(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(as_engine);
    auto* as_type_info = LookupCachedTypeInfo(as_engine, type);
    auto* as_array = ScriptArray::Create(as_type_info);
    FO_RUNTIME_ASSERT(as_array);
    return as_array;
}

auto CreateScriptDict(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(as_engine);
    auto* as_type_info = LookupCachedTypeInfo(as_engine, type);
    auto* as_dict = ScriptDict::Create(as_type_info);
    FO_RUNTIME_ASSERT(as_dict);
    return as_dict;
}

auto CalcConstructAddrSpace(const Property* prop) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeProtoReference()) {
            return sizeof(Entity*);
        }
        else if (prop->IsBaseTypeHash()) {
            return sizeof(hstring);
        }
        else if (prop->IsBaseTypeEnum()) {
            return sizeof(int32_t);
        }
        else if (prop->IsBaseTypePrimitive()) {
            return prop->GetBaseSize();
        }
        else if (prop->IsBaseTypeStruct()) {
            return prop->GetBaseSize();
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (prop->IsBaseTypeRefType()) {
        return sizeof(void*);
    }
    else if (prop->IsString()) {
        return sizeof(string);
    }
    else if (prop->IsArray()) {
        return sizeof(ScriptArray*);
    }
    else if (prop->IsDict()) {
        return sizeof(ScriptDict*);
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

void FreeConstructAddrSpace(const Property* prop, void* construct_addr)
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            cast_from_void<hstring*>(construct_addr)->~hstring();
        }
    }
    else if (prop->IsBaseTypeRefType()) {
        auto* ref_obj = *static_cast<void**>(construct_addr);

        if (ref_obj != nullptr) {
            cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
        }
    }
    else if (prop->IsString()) {
        cast_from_void<string*>(construct_addr)->~string();
    }
    else if (prop->IsArray()) {
        (*cast_from_void<ScriptArray**>(construct_addr))->Release();
    }
    else if (prop->IsDict()) {
        (*cast_from_void<ScriptDict**>(construct_addr))->Release();
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

void ConvertPropsToScriptObject(const Property* prop, PropertyRawData& prop_data, void* construct_addr, AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    const auto resolve_hash = [prop](const void* hptr) -> hstring {
        const auto hash = *cast_from_void<const hstring::hash_t*>(hptr);
        return hash ? prop->GetRegistrator()->GetHashResolver()->ResolveHash(hash) : hstring();
    };

    const auto resolve_fixed_type = [prop, as_engine, &resolve_hash](const void* hptr) -> Entity* {
        const auto pid = resolve_hash(hptr);

        if (!pid) {
            return nullptr;
        }

        const auto* engine = GetGameEngine(as_engine);
        const auto type_name = engine->Hashes.ToHashedString(prop->GetBaseTypeName());
        const auto* proto = engine->GetProtoEntity(type_name, pid);

        if (proto == nullptr) {
            throw ScriptException("Unable to resolve proto", prop->GetName(), pid);
        }

        return const_cast<ProtoEntity*>(proto);
    };

    const auto resolve_enum = [](const void* eptr, size_t elen) -> int32_t {
        int32_t result = 0;
        MemCopy(&result, eptr, elen);
        return result;
    };

    const auto* data = prop_data.GetPtrAs<uint8_t>();
    const auto data_size = prop_data.GetSize();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeProtoReference()) {
            FO_RUNTIME_ASSERT(data_size == sizeof(hstring::hash_t));
            new (construct_addr) Entity*(resolve_fixed_type(data));
        }
        else if (prop->IsBaseTypeHash()) {
            FO_RUNTIME_ASSERT(data_size == sizeof(hstring::hash_t));
            new (construct_addr) hstring(resolve_hash(data));
        }
        else if (prop->IsBaseTypeEnum()) {
            FO_RUNTIME_ASSERT(data_size != 0);
            FO_RUNTIME_ASSERT(data_size <= sizeof(int32_t));
            MemFill(construct_addr, 0, sizeof(int32_t));
            MemCopy(construct_addr, data, data_size);
        }
        else if (prop->IsBaseTypePrimitive()) {
            FO_RUNTIME_ASSERT(data_size != 0);
            MemCopy(construct_addr, data, data_size);
        }
        else if (prop->IsBaseTypeStruct()) {
            FO_RUNTIME_ASSERT(data_size != 0);
            MemCopy(construct_addr, data, data_size);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (prop->IsBaseTypeRefType() && !prop->IsArray() && !prop->IsDict()) {
        *static_cast<void**>(construct_addr) = CreateRefTypeScriptObjectFromProperty(prop, {data, data_size});
    }
    else if (prop->IsString()) {
        new (construct_addr) string(reinterpret_cast<const char*>(data), data_size);
    }
    else if (prop->IsArray()) {
        auto* arr = ScriptArray::Create(LookupCachedTypeInfoForProperty(as_engine, prop));
        FO_RUNTIME_ASSERT(arr);

        if (prop->IsArrayOfString()) {
            if (data_size != 0) {
                uint32_t arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);

                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    uint32_t str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&str));

                    data += str_size;
                }
            }
        }
        else if (prop->IsBaseTypeRefType()) {
            if (data_size != 0) {
                uint32_t arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);

                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    uint32_t ref_data_size;
                    MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                    data += sizeof(ref_data_size);

                    auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                    arr->SetValue(numeric_cast<int32_t>(i), &ref_obj);

                    if (ref_obj != nullptr) {
                        cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                    }

                    data += ref_data_size;
                }
            }
        }
        else if (prop->IsBaseTypeProtoReference()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto* fixed_type = resolve_fixed_type(data);
                    arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&fixed_type));

                    data += sizeof(hstring::hash_t);
                }
            }
        }
        else if (prop->IsBaseTypeHash()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    const auto hvalue = resolve_hash(data);
                    arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&hvalue));

                    data += sizeof(hstring::hash_t);
                }
            }
        }
        else if (prop->IsBaseTypeEnum()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                if (prop->GetBaseSize() == sizeof(int32_t)) {
                    MemCopy(arr->At(0), data, data_size);
                }
                else {
                    for (uint32_t i = 0; i < arr_size; i++) {
                        MemCopy(arr->At(numeric_cast<int32_t>(i)), data + i * prop->GetBaseSize(), prop->GetBaseSize());
                    }
                }
            }
        }
        else if (prop->IsBaseTypePrimitive()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));
                MemCopy(arr->At(0), data, data_size);
            }
        }
        else if (prop->IsBaseTypeStruct()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    MemCopy(arr->At(numeric_cast<int32_t>(i)), data, prop->GetBaseSize());
                    data += prop->GetBaseSize();
                }
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }

        *cast_from_void<ScriptArray**>(construct_addr) = arr;
    }
    else if (prop->IsDict()) {
        ScriptDict* dict = ScriptDict::Create(LookupCachedTypeInfoForProperty(as_engine, prop));
        FO_RUNTIME_ASSERT(dict);

        if (data_size != 0) {
            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;
                const auto inner_array_type_name = strex("array<{}{}>", prop->GetBaseTypeName(), prop->IsBaseTypeRefType() ? "@" : "").str();
                auto* inner_array_type_info = LookupCachedTypeInfo(as_engine, inner_array_type_name.c_str());

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    uint32_t arr_size;
                    MemCopy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(arr_size);

                    auto* arr = ScriptArray::Create(inner_array_type_info);
                    FO_RUNTIME_ASSERT(arr);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                uint32_t str_size;
                                MemCopy(&str_size, data, sizeof(str_size));
                                data += sizeof(str_size);

                                string str(reinterpret_cast<const char*>(data), str_size);
                                arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&str));
                                data += str_size;
                            }
                        }
                        else if (prop->IsBaseTypeRefType()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                uint32_t ref_data_size;
                                MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                                data += sizeof(ref_data_size);

                                auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                                arr->SetValue(numeric_cast<int32_t>(i), &ref_obj);

                                if (ref_obj != nullptr) {
                                    cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                                }

                                data += ref_data_size;
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto* fixed_type = resolve_fixed_type(data);
                                arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&fixed_type));

                                data += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                const auto hvalue = resolve_hash(data);
                                arr->SetValue(numeric_cast<int32_t>(i), cast_to_void(&hvalue));

                                data += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            if (prop->GetBaseSize() == sizeof(int32_t)) {
                                MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            }
                            else {
                                for (uint32_t i = 0; i < arr_size; i++) {
                                    MemCopy(arr->At(numeric_cast<int32_t>(i)), data + i * prop->GetBaseSize(), prop->GetBaseSize());
                                }
                            }

                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                MemCopy(arr->At(numeric_cast<int32_t>(i)), data, prop->GetBaseSize());
                                data += prop->GetBaseSize();
                            }
                        }
                        else {
                            FO_UNREACHABLE_PLACE();
                        }
                    }

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};
                        dict->Set(cast_to_void(&key_str), cast_to_void(&arr));
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(cast_to_void(&hkey), cast_to_void(&arr));
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeyTypeSize());
                        dict->Set(cast_to_void(&ekey), cast_to_void(&arr));
                    }
                    else {
                        dict->Set(cast_to_void(key), cast_to_void(&arr));
                    }

                    arr->Release();
                }
            }
            else if (prop->IsDictOfString()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    uint32_t str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint32_t);

                    auto str = string {reinterpret_cast<const char*>(data), str_size};

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};
                        dict->Set(cast_to_void(&key_str), cast_to_void(&str));
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(cast_to_void(&hkey), cast_to_void(&str));
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeyTypeSize());
                        dict->Set(cast_to_void(&ekey), cast_to_void(&str));
                    }
                    else {
                        dict->Set(cast_to_void(key), cast_to_void(&str));
                    }

                    data += str_size;
                }
            }
            else {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = *reinterpret_cast<const uint32_t*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};

                        if (prop->IsBaseTypeRefType()) {
                            uint32_t ref_data_size;
                            MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                            data += sizeof(ref_data_size);

                            auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                            dict->Set(cast_to_void(&key_str), &ref_obj);

                            if (ref_obj != nullptr) {
                                cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            auto* fixed_type = resolve_fixed_type(data);
                            dict->Set(cast_to_void(&key_str), cast_to_void(&fixed_type));
                        }
                        else if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&key_str), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&key_str), cast_to_void(data));
                        }
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);

                        if (prop->IsBaseTypeRefType()) {
                            uint32_t ref_data_size;
                            MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                            data += sizeof(ref_data_size);

                            auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                            dict->Set(cast_to_void(&hkey), &ref_obj);

                            if (ref_obj != nullptr) {
                                cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            auto* fixed_type = resolve_fixed_type(data);
                            dict->Set(cast_to_void(&hkey), cast_to_void(&fixed_type));
                        }
                        else if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&hkey), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&hkey), cast_to_void(data));
                        }
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeyTypeSize());

                        if (prop->IsBaseTypeRefType()) {
                            uint32_t ref_data_size;
                            MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                            data += sizeof(ref_data_size);

                            auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                            dict->Set(cast_to_void(&ekey), &ref_obj);

                            if (ref_obj != nullptr) {
                                cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            auto* fixed_type = resolve_fixed_type(data);
                            dict->Set(cast_to_void(&ekey), cast_to_void(&fixed_type));
                        }
                        else if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&ekey), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&ekey), cast_to_void(data));
                        }
                    }
                    else {
                        if (prop->IsBaseTypeRefType()) {
                            uint32_t ref_data_size;
                            MemCopy(&ref_data_size, data, sizeof(ref_data_size));
                            data += sizeof(ref_data_size);

                            auto* ref_obj = CreateRefTypeScriptObjectFromProperty(prop, {data, ref_data_size});
                            dict->Set(cast_to_void(key), &ref_obj);

                            if (ref_obj != nullptr) {
                                cast_from_void<DynamicRefTypeInstance*>(ref_obj)->Release();
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            auto* fixed_type = resolve_fixed_type(data);
                            dict->Set(cast_to_void(key), cast_to_void(&fixed_type));
                        }
                        else if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(key), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(key), cast_to_void(data));
                        }
                    }

                    if (!prop->IsBaseTypeRefType()) {
                        data += prop->GetBaseSize();
                    }
                }
            }
        }

        *cast_from_void<ScriptDict**>(construct_addr) = dict;
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

auto ConvertScriptToPropsObject(const Property* prop, void* as_obj) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    PropertyRawData prop_data;

    const auto resolve_proto_hash = [](const Entity* entity) -> hstring::hash_t {
        if (entity != nullptr) {
            const auto* proto_entity = dynamic_cast<const ProtoEntity*>(entity);
            FO_RUNTIME_ASSERT(proto_entity);
            return proto_entity->GetProtoId().as_hash();
        }
        else {
            return {};
        }
    };

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeProtoReference()) {
            const auto* entity = *cast_from_void<Entity**>(as_obj);
            const auto pid = resolve_proto_hash(entity);
            FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(pid));
            prop_data.SetAs<hstring::hash_t>(pid);
        }
        else if (prop->IsBaseTypeHash()) {
            const auto hash = cast_from_void<const hstring*>(as_obj)->as_hash();
            FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hash));
            prop_data.SetAs<hstring::hash_t>(hash);
        }
        else {
            prop_data.Set(as_obj, prop->GetBaseSize());
        }
    }
    else if (prop->IsBaseTypeRefType() && !prop->IsArray() && !prop->IsDict()) {
        auto* ref_obj = *static_cast<void**>(as_obj);

        if (ref_obj != nullptr) {
            prop_data = ConvertRefTypeScriptObjectToProperty(prop, ref_obj);
        }
    }
    else if (prop->IsString()) {
        const auto& str = *cast_from_void<const string*>(as_obj);
        prop_data.Pass(str.data(), str.length());
    }
    else if (prop->IsArray()) {
        const auto* arr = *cast_from_void<ScriptArray**>(as_obj);

        if (prop->IsArrayOfString()) {
            const auto arr_size = numeric_cast<uint32_t>(arr != nullptr ? arr->GetSize() : 0);

            if (arr_size != 0) {
                // Calculate size
                size_t data_size = sizeof(uint32_t);

                for (uint32_t i = 0; i < arr_size; i++) {
                    const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32_t>(i)));
                    data_size += sizeof(uint32_t) + numeric_cast<uint32_t>(str.length());
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                MemCopy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint32_t);

                for (uint32_t i = 0; i < arr_size; i++) {
                    const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32_t>(i)));

                    const auto str_size = numeric_cast<uint32_t>(str.length());
                    MemCopy(buf, &str_size, sizeof(str_size));
                    buf += sizeof(str_size);

                    if (str_size != 0) {
                        MemCopy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else {
            const uint32_t arr_size = (arr != nullptr ? arr->GetSize() : 0);
            const size_t data_size = [&]() -> size_t {
                if (arr == nullptr || arr_size == 0) {
                    return 0;
                }

                if (prop->IsBaseTypeRefType()) {
                    size_t size = sizeof(uint32_t);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(arr->At(numeric_cast<int32_t>(i))));
                        size += sizeof(uint32_t) + ref_data.GetSize();
                    }

                    return size;
                }

                return numeric_cast<size_t>(arr_size * prop->GetBaseSize());
            }();

            if (data_size != 0) {
                if (prop->IsBaseTypeRefType()) {
                    auto* buf = prop_data.Alloc(data_size);
                    MemCopy(buf, &arr_size, sizeof(arr_size));
                    buf += sizeof(arr_size);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(arr->At(numeric_cast<int32_t>(i))));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        MemCopy(buf, &ref_data_size, sizeof(ref_data_size));
                        buf += sizeof(ref_data_size);

                        if (ref_data_size != 0) {
                            MemCopy(buf, ref_data.GetPtr(), ref_data.GetSize());
                            buf += ref_data.GetSize();
                        }
                    }
                }
                else if (prop->IsBaseTypeProtoReference()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        const auto* entity = *cast_from_void<Entity**>(arr->At(numeric_cast<int32_t>(i)));
                        const auto pid = resolve_proto_hash(entity);
                        MemCopy(buf, &pid, sizeof(hstring::hash_t));
                        buf += prop->GetBaseSize();
                    }
                }
                else if (prop->IsBaseTypeHash()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        const auto hash = cast_from_void<const hstring*>(arr->At(numeric_cast<int32_t>(i)))->as_hash();
                        MemCopy(buf, &hash, sizeof(hstring::hash_t));
                        buf += prop->GetBaseSize();
                    }
                }
                else if (prop->IsBaseTypeEnum()) {
                    if (prop->GetBaseSize() == sizeof(int32_t)) {
                        prop_data.Pass(arr->At(0), data_size);
                    }
                    else {
                        auto* buf = prop_data.Alloc(data_size);

                        for (uint32_t i = 0; i < arr_size; i++) {
                            MemCopy(buf, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                            buf += prop->GetBaseSize();
                        }
                    }
                }
                else if (prop->IsBaseTypePrimitive()) {
                    prop_data.Pass(arr->At(0), data_size);
                }
                else if (prop->IsBaseTypeStruct()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        MemCopy(buf, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                        buf += prop->GetBaseSize();
                    }
                }
                else {
                    FO_UNREACHABLE_PLACE();
                }
            }
        }
    }
    else if (prop->IsDict()) {
        const auto* dict = *cast_from_void<ScriptDict**>(as_obj);

        if (prop->IsDictOfArray()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : dict->GetMap()) {
                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *cast_from_void<const string*>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    const auto* arr = *cast_from_void<const ScriptArray**>(value);
                    const uint32_t arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    data_size += sizeof(arr_size);

                    if (prop->IsDictOfArrayOfString()) {
                        for (uint32_t i = 0; i < arr_size; i++) {
                            const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32_t>(i)));
                            data_size += sizeof(uint32_t) + numeric_cast<uint32_t>(str.length());
                        }
                    }
                    else if (prop->IsBaseTypeRefType()) {
                        for (uint32_t i = 0; i < arr_size; i++) {
                            auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(arr->At(numeric_cast<int32_t>(i))));
                            data_size += sizeof(uint32_t) + ref_data.GetSize();
                        }
                    }
                    else {
                        data_size += arr_size * prop->GetBaseSize();
                    }
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto* arr = *cast_from_void<const ScriptArray**>(value);

                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *cast_from_void<const string*>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str.length());
                        MemCopy(buf, &key_len, sizeof(key_len));
                        buf += sizeof(key_len);
                        MemCopy(buf, key_str.c_str(), key_len);
                        buf += key_len;
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = cast_from_void<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }

                    const uint32_t arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    MemCopy(buf, &arr_size, sizeof(uint32_t));
                    buf += sizeof(arr_size);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32_t>(i)));
                                const auto str_size = numeric_cast<uint32_t>(str.length());

                                MemCopy(buf, &str_size, sizeof(uint32_t));
                                buf += sizeof(str_size);

                                if (str_size != 0) {
                                    MemCopy(buf, str.c_str(), str_size);
                                    buf += str_size;
                                }
                            }
                        }
                        else if (prop->IsBaseTypeRefType()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(arr->At(numeric_cast<int32_t>(i))));
                                const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                                MemCopy(buf, &ref_data_size, sizeof(ref_data_size));
                                buf += sizeof(ref_data_size);

                                if (ref_data_size != 0) {
                                    MemCopy(buf, ref_data.GetPtr(), ref_data.GetSize());
                                    buf += ref_data.GetSize();
                                }
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                const auto* entity = *cast_from_void<Entity**>(arr->At(numeric_cast<int32_t>(i)));
                                const auto pid = resolve_proto_hash(entity);
                                MemCopy(buf, &pid, sizeof(hstring::hash_t));
                                buf += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                const auto hash = cast_from_void<const hstring*>(arr->At(numeric_cast<int32_t>(i)))->as_hash();
                                MemCopy(buf, &hash, sizeof(hstring::hash_t));
                                buf += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            if (prop->GetBaseSize() == sizeof(int32_t)) {
                                MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                                buf += arr_size * prop->GetBaseSize();
                            }
                            else {
                                for (uint32_t i = 0; i < arr_size; i++) {
                                    MemCopy(buf, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                                    buf += prop->GetBaseSize();
                                }
                            }
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                            buf += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                MemCopy(buf, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                                buf += prop->GetBaseSize();
                            }
                        }
                        else {
                            FO_UNREACHABLE_PLACE();
                        }
                    }
                }
            }
        }
        else if (prop->IsDictOfString()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : dict->GetMap()) {
                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *cast_from_void<const string*>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    const auto& str = *cast_from_void<const string*>(value);
                    const auto str_size = numeric_cast<uint32_t>(str.length());

                    data_size += sizeof(str_size) + str_size;
                }

                // Make buffer
                uint8_t* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto& str = *cast_from_void<const string*>(value);

                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *cast_from_void<const string*>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str.length());
                        MemCopy(buf, &key_len, sizeof(key_len));
                        buf += sizeof(key_len);
                        MemCopy(buf, key_str.c_str(), key_len);
                        buf += key_len;
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = cast_from_void<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }

                    const auto str_size = numeric_cast<uint32_t>(str.length());
                    MemCopy(buf, &str_size, sizeof(uint32_t));
                    buf += sizeof(str_size);

                    if (str_size != 0) {
                        MemCopy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else if (prop->IsDictKeyString()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto& key_str = *cast_from_void<const string*>(key);
                    const auto key_len = numeric_cast<uint32_t>(key_str.length());
                    data_size += sizeof(key_len) + key_len;

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(value));
                        data_size += sizeof(uint32_t) + ref_data.GetSize();
                    }
                    else {
                        data_size += prop->GetBaseSize();
                    }
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto& key_str = *cast_from_void<const string*>(key);
                    const auto key_len = numeric_cast<uint32_t>(key_str.length());

                    MemCopy(buf, &key_len, sizeof(key_len));
                    buf += sizeof(key_len);
                    MemCopy(buf, key_str.c_str(), key_len);
                    buf += key_len;

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(value));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        MemCopy(buf, &ref_data_size, sizeof(ref_data_size));
                        buf += sizeof(ref_data_size);

                        if (ref_data_size != 0) {
                            MemCopy(buf, ref_data.GetPtr(), ref_data.GetSize());
                            buf += ref_data.GetSize();
                        }
                    }
                    else if (prop->IsBaseTypeProtoReference()) {
                        const auto* entity = *cast_from_void<Entity**>(value);
                        const auto pid = resolve_proto_hash(entity);
                        MemCopy(buf, &pid, prop->GetBaseSize());
                    }
                    else if (prop->IsBaseTypeHash()) {
                        const auto hash = cast_from_void<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, prop->GetBaseSize());
                    }
                    else {
                        MemCopy(buf, value, prop->GetBaseSize());
                    }

                    if (!prop->IsBaseTypeRefType()) {
                        buf += prop->GetBaseSize();
                    }
                }
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeyTypeSize();
            const auto data_size = [&]() {
                if (dict == nullptr || dict->GetSize() == 0) {
                    return size_t {};
                }

                if (!prop->IsBaseTypeRefType()) {
                    return numeric_cast<size_t>(dict->GetSize()) * (key_element_size + prop->GetBaseSize());
                }

                size_t size = 0;

                for (auto&& [key, value] : dict->GetMap()) {
                    ignore_unused(key);
                    size += key_element_size;
                    auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(value));
                    size += sizeof(uint32_t) + ref_data.GetSize();
                }

                return size;
            }();

            if (data_size != 0) {
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    if (prop->IsDictKeyHash()) {
                        const auto hkey = cast_from_void<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, key_element_size);
                    }
                    else {
                        MemCopy(buf, key, key_element_size);
                    }

                    buf += key_element_size;

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, *static_cast<void**>(value));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        MemCopy(buf, &ref_data_size, sizeof(ref_data_size));
                        buf += sizeof(ref_data_size);

                        if (ref_data_size != 0) {
                            MemCopy(buf, ref_data.GetPtr(), ref_data.GetSize());
                            buf += ref_data.GetSize();
                        }
                    }
                    else if (prop->IsBaseTypeProtoReference()) {
                        const auto* entity = *cast_from_void<Entity**>(value);
                        const auto pid = resolve_proto_hash(entity);
                        MemCopy(buf, &pid, prop->GetBaseSize());
                    }
                    else if (prop->IsBaseTypeHash()) {
                        const auto hash = cast_from_void<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, prop->GetBaseSize());
                    }
                    else {
                        MemCopy(buf, value, prop->GetBaseSize());
                    }

                    if (!prop->IsBaseTypeRefType()) {
                        buf += prop->GetBaseSize();
                    }
                }
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    return prop_data;
}

auto GetScriptObjectInfo(const void* ptr, int32_t type_id) -> string
{
    FO_STACK_TRACE_ENTRY();

    switch (type_id) {
    case AngelScript::asTYPEID_VOID:
        return "void";
    case AngelScript::asTYPEID_BOOL:
        return strex("bool: {}", *cast_from_void<const bool*>(ptr) ? "true" : "false");
    case AngelScript::asTYPEID_INT8:
        return strex("int8: {}", *cast_from_void<const int8_t*>(ptr));
    case AngelScript::asTYPEID_INT16:
        return strex("int16: {}", *cast_from_void<const int16_t*>(ptr));
    case AngelScript::asTYPEID_INT32:
        return strex("int32: {}", *cast_from_void<const int32_t*>(ptr));
    case AngelScript::asTYPEID_INT64:
        return strex("int64: {}", *cast_from_void<const int64_t*>(ptr));
    case AngelScript::asTYPEID_UINT8:
        return strex("uint8: {}", *cast_from_void<const uint8_t*>(ptr));
    case AngelScript::asTYPEID_UINT16:
        return strex("uint16: {}", *cast_from_void<const uint16_t*>(ptr));
    case AngelScript::asTYPEID_UINT32:
        return strex("uint32: {}", *cast_from_void<const uint32_t*>(ptr));
    case AngelScript::asTYPEID_UINT64:
        return strex("uint64: {}", *cast_from_void<const uint64_t*>(ptr));
    case AngelScript::asTYPEID_FLOAT:
        return strex("float32: {}", *cast_from_void<const float32_t*>(ptr));
    case AngelScript::asTYPEID_DOUBLE:
        return strex("float64: {}", *cast_from_void<const float64_t*>(ptr));
    default:
        break;
    }

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);
    auto* as_engine = ctx->GetEngine();
    const auto* as_type_info = as_engine->GetTypeInfoById(type_id);
    FO_RUNTIME_ASSERT(as_type_info);
    const string type_name = as_type_info->GetName();
    const auto* meta = GetEngineMetadata(as_engine);

    if (type_name == "string") {
        return strex("string: {}", *cast_from_void<const string*>(ptr));
    }
    if (type_name == "hstring") {
        return strex("hstring: {}", *cast_from_void<const hstring*>(ptr));
    }
    if (type_name == "any") {
        return strex("any: {}", *cast_from_void<const any_t*>(ptr));
    }
    if (type_name == "ident") {
        return strex("ident: {}", *cast_from_void<const ident_t*>(ptr));
    }
    if (type_name == "timespan") {
        return strex("timespan: {}", *cast_from_void<const timespan*>(ptr));
    }
    if (type_name == "nanotime") {
        return strex("nanotime: {}", *cast_from_void<const nanotime*>(ptr));
    }
    if (type_name == "synctime") {
        return strex("synctime: {}", *cast_from_void<const synctime*>(ptr));
    }

    if (const auto enum_value_count = as_type_info->GetEnumValueCount(); enum_value_count != 0) {
        int32_t enum_value = 0;

        if (meta->IsValidBaseType(type_name)) {
            const auto& enum_type = meta->GetBaseType(type_name);
            enum_value = ReadEnumValueAsInt32(ptr, enum_type);

            bool failed = false;
            const string& enum_value_name = meta->ResolveEnumValueName(type_name, enum_value, &failed);

            if (!failed) {
                return strex("{}: {}", type_name, enum_value_name);
            }
        }
        else {
            enum_value = *cast_from_void<const int32_t*>(ptr);
        }

        for (AngelScript::asUINT i = 0; i < enum_value_count; i++) {
            AngelScript::asINT64 check_enum_value = 0;
            const char* enum_value_name = as_type_info->GetEnumValueByIndex(i, &check_enum_value);

            if (check_enum_value == enum_value) {
                return strex("{}: {}", type_name, enum_value_name);
            }
        }

        return strex("{}: {}", type_name, enum_value);
    }

    return strex("{}", type_name);
}

auto GetScriptFuncName(const AngelScript::asIScriptFunction* func, HashResolver& hash_resolver) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(func);

    string func_name;

    if (func->GetNamespace() == nullptr) {
        func_name = strex("{}", func->GetName()).str();
    }
    else {
        func_name = strex("{}::{}", func->GetNamespace(), func->GetName()).str();
    }

    return hash_resolver.ToHashedString(func_name);
}

auto IsScriptNamespaceAllowed(string_view ns, const vector<string>& allowed_namespaces) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (allowed_namespaces.empty() || ns.empty()) {
        return false;
    }

    for (const auto& allowed : allowed_namespaces) {
        if (allowed.empty()) {
            continue;
        }

        if (ns.starts_with(allowed)) {
            return true;
        }
    }

    return false;
}

auto ReadEnumValueAsInt32(const void* ptr, const BaseTypeDesc& enum_type) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ptr);
    FO_RUNTIME_ASSERT(enum_type.IsEnum);
    FO_RUNTIME_ASSERT(enum_type.EnumUnderlyingType);
    FO_RUNTIME_ASSERT(enum_type.EnumUnderlyingType->IsInt);

    if (enum_type.EnumUnderlyingType->IsSignedInt) {
        switch (enum_type.Size) {
        case sizeof(int8_t):
            return *cast_from_void<const int8_t*>(ptr);
        case sizeof(int16_t):
            return *cast_from_void<const int16_t*>(ptr);
        case sizeof(int32_t):
            return *cast_from_void<const int32_t*>(ptr);
        default:
            break;
        }
    }
    else {
        switch (enum_type.Size) {
        case sizeof(uint8_t):
            return *cast_from_void<const uint8_t*>(ptr);
        case sizeof(uint16_t):
            return *cast_from_void<const uint16_t*>(ptr);
        case sizeof(uint32_t):
            return static_cast<int32_t>(*cast_from_void<const uint32_t*>(ptr));
        default:
            break;
        }
    }

    throw GenericException("Unsupported enum underlying size", enum_type.Name, enum_type.Size);
}

void WriteEnumValueFromInt32(void* ptr, const BaseTypeDesc& enum_type, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(ptr);
    FO_RUNTIME_ASSERT(enum_type.IsEnum);
    FO_RUNTIME_ASSERT(enum_type.EnumUnderlyingType);
    FO_RUNTIME_ASSERT(enum_type.EnumUnderlyingType->IsInt);

    if (enum_type.EnumUnderlyingType->IsSignedInt) {
        switch (enum_type.Size) {
        case sizeof(int8_t):
            *cast_from_void<int8_t*>(ptr) = numeric_cast<int8_t>(value);
            return;
        case sizeof(int16_t):
            *cast_from_void<int16_t*>(ptr) = numeric_cast<int16_t>(value);
            return;
        case sizeof(int32_t):
            *cast_from_void<int32_t*>(ptr) = value;
            return;
        default:
            break;
        }
    }
    else {
        switch (enum_type.Size) {
        case sizeof(uint8_t):
            *cast_from_void<uint8_t*>(ptr) = numeric_cast<uint8_t>(value);
            return;
        case sizeof(uint16_t):
            *cast_from_void<uint16_t*>(ptr) = numeric_cast<uint16_t>(value);
            return;
        case sizeof(uint32_t):
            *cast_from_void<uint32_t*>(ptr) = static_cast<uint32_t>(value);
            return;
        default:
            break;
        }
    }

    throw GenericException("Unsupported enum underlying size", enum_type.Name, enum_type.Size);
}

auto CreateRefTypeScriptObjectFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(base_type.IsRefType);
    FO_RUNTIME_ASSERT(base_type.RefType);
    FO_RUNTIME_ASSERT(base_type.RefType->FieldsRegistrator);

    auto ref_instance = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(base_type.RefType->FieldsRegistrator.get());
    ref_instance->LoadFromRawData(base_type, raw_data);

    return ref_instance.release_ownership();
}

auto ConvertRefTypeScriptObjectToRawData(const BaseTypeDesc& base_type, void* as_obj) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(base_type.IsRefType);
    FO_RUNTIME_ASSERT(base_type.RefType);
    FO_RUNTIME_ASSERT(base_type.RefType->FieldsRegistrator);

    if (as_obj == nullptr) {
        return {};
    }

    auto* ref_instance = cast_from_void<DynamicRefTypeInstance*>(as_obj);
    return ref_instance->GetSerializedRawData(base_type);
}

auto CreateRefTypeScriptObjectFromProperty(const Property* prop, span<const uint8_t> raw_data) -> void*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsBaseTypeRefType());

    return CreateRefTypeScriptObjectFromRawData(prop->GetBaseType(), raw_data);
}

auto ConvertRefTypeScriptObjectToProperty(const Property* prop, void* as_obj) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(prop->IsBaseTypeRefType());
    const auto raw_data = ConvertRefTypeScriptObjectToRawData(prop->GetBaseType(), as_obj);

    PropertyRawData prop_data;

    if (!raw_data.empty()) {
        prop_data.Set(raw_data.data(), raw_data.size());
    }

    return prop_data;
}

FO_END_NAMESPACE

#endif
