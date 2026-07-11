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
    unordered_map<string, ptr<AngelScript::asITypeInfo>> Map {};
    unordered_map<ptr<const Property>, ptr<AngelScript::asITypeInfo>> ByProperty {};
};

void SetScriptTypeFastCompare(ptr<AngelScript::asITypeInfo> type, ScriptFastCompareFunc func)
{
    FO_STACK_TRACE_ENTRY();

    type->SetUserData(std::bit_cast<void*>(func), AS_TYPE_FAST_COMPARE_USER_DATA);
}

auto GetScriptTypeFastCompare(ptr<const AngelScript::asITypeInfo> type) -> ScriptFastCompareFunc
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> user_data = type->GetUserData(AS_TYPE_FAST_COMPARE_USER_DATA);
    return user_data ? std::bit_cast<ScriptFastCompareFunc>(user_data.get()) : nullptr;
}

[[noreturn]] void ThrowScriptCoreException(string_view file, int32_t line, int32_t result)
{
    FO_NO_STACK_TRACE_ENTRY();

    const string_view file_name = strvex(file).extract_file_name().erase_file_extension();
    throw ScriptCoreException(strex("File: {}", file_name), strex("Line: {}", line), strex("Result: {}", result));
}

template<typename T>
static auto ScriptEngineUserDataAs(ptr<AngelScript::asIScriptEngine> as_engine, AngelScript::asPWORD type = 0) noexcept -> nptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<T*>(as_engine->GetUserData(type));
}

auto GetScriptBackend(ptr<BaseEngine> engine) -> ptr<AngelScriptBackend>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto backend = engine->GetBackend<AngelScriptBackend>(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX);
    FO_VERIFY_AND_THROW(backend, "Missing AngelScript backend");
    return backend;
}

auto GetScriptBackend(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<AngelScriptBackend>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto backend = ScriptEngineUserDataAs<AngelScriptBackend>(as_engine);
    FO_VERIFY_AND_THROW(backend, "Missing AngelScript backend");
    return backend;
}

auto GetEngineMetadata(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<const EngineMetadata>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto backend = ScriptEngineUserDataAs<AngelScriptBackend>(as_engine);
    FO_VERIFY_AND_THROW(backend, "Missing AngelScript backend");
    auto meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Missing engine metadata");
    return meta;
}

auto GetGameEngine(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<BaseEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto backend = ScriptEngineUserDataAs<AngelScriptBackend>(as_engine);
    FO_VERIFY_AND_THROW(backend, "Missing AngelScript backend");
    return backend->GetGameEngine();
}

void CheckScriptEntityNonNull(nptr<const Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!entity) {
        throw ScriptException("Access to null entity");
    }
}

void CheckScriptEntityNonDestroyed(nptr<const Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity) {
        if (entity->IsDestroyed()) {
            throw ScriptException("Access to destroyed entity");
        }
    }
}

void CheckScriptEntityAccessAndNonDestroyed(nptr<const Entity> entity)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (entity) {
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

    FO_VERIFY_AND_THROW(type, "Missing type descriptor");

    string result;

    if (type.Kind == ComplexTypeKind::DictOfArray) {
        FO_VERIFY_AND_THROW(type.KeyType, "Dictionary key type is not set");
        result = strex("dict<{},array<{}>>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        FO_VERIFY_AND_THROW(type.KeyType, "Dictionary key type is not set");
        result = strex("dict<{},{}>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Array) {
        result = strex("array<{}>", MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Callback) {
        result = "callback";

        for (size_t i = 0; i < type.CallbackArgs->size(); i++) {
            auto cb_arg = make_ptr(&(*type.CallbackArgs)[i]);
            result += "_";

            if (cb_arg->Kind == ComplexTypeKind::None) {
                result += "void";
            }
            else if (cb_arg->Kind == ComplexTypeKind::DictOfArray) {
                FO_VERIFY_AND_THROW(cb_arg->KeyType, "Callback argument key type is not set");
                result += strex("{}_to_{}_arr", cb_arg->KeyType.value().Name, cb_arg->BaseType.Name);
            }
            else if (cb_arg->Kind == ComplexTypeKind::Dict) {
                FO_VERIFY_AND_THROW(cb_arg->KeyType, "Callback argument key type is not set");
                result += strex("{}_to_{}", cb_arg->KeyType.value().Name, cb_arg->BaseType.Name);
            }
            else if (cb_arg->Kind == ComplexTypeKind::Array) {
                result += strex("{}_arr", cb_arg->BaseType.Name);
            }
            else {
                result += cb_arg->BaseType.Name;
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

    FO_VERIFY_AND_THROW(type, "Missing type descriptor");

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
        auto arg = make_ptr(&args[i]);

        if (i > 0) {
            result += ", ";
        }

        result += MakeScriptArgName(arg->Type, arg->Nullable);
        result += " ";
        result += arg->Name;

        if (!arg->DefaultValue.empty()) {
            result += " = ";
            result += arg->DefaultValue;
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
            FO_VERIFY_AND_THROW(result.back() == '+', "AngelScript type declaration suffix is malformed", result);

            if (pass_ownership) {
                result.pop_back();
            }
        }
    }
    else {
        FO_VERIFY_AND_THROW(result.back() == '+', "AngelScript type declaration suffix is malformed", result);
        result.pop_back();
    }

    return result;
}

auto MakeScriptPropertyName(ptr<const Property> prop) -> string
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

static void CleanupScriptTypeInfoCache(ptr<ScriptTypeInfoCache> cache) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_cache = adopt_unique_ptr(cache);
    ignore_unused(owned_cache);
}

static void CleanupTypeInfoCache(AngelScript::asIScriptEngine* engine) noexcept
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = engine;
    auto cache = ScriptEngineUserDataAs<ScriptTypeInfoCache>(as_engine, AS_TYPE_INFO_CACHE_USER_DATA);

    if (cache) {
        CleanupScriptTypeInfoCache(cache);
    }

    as_engine->SetUserData(nullptr, AS_TYPE_INFO_CACHE_USER_DATA);
}

static auto GetTypeInfoCache(ptr<AngelScript::asIScriptEngine> as_engine) -> ptr<ScriptTypeInfoCache>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto cache = ScriptEngineUserDataAs<ScriptTypeInfoCache>(as_engine, AS_TYPE_INFO_CACHE_USER_DATA);

    if (!cache) {
        auto cache_owner = SafeAlloc::MakeUnique<ScriptTypeInfoCache>();
        auto cache_ptr = cache_owner.release();
        as_engine->SetUserData(make_nptr(cache_ptr.get()).void_cast(), AS_TYPE_INFO_CACHE_USER_DATA);
        as_engine->SetEngineUserDataCleanupCallback(CleanupTypeInfoCache, AS_TYPE_INFO_CACHE_USER_DATA);
        cache = cache_ptr;
    }

    return cache;
}

static auto LookupCachedTypeInfo(ptr<AngelScript::asIScriptEngine> as_engine, string_view type) -> ptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    const string type_key {type};
    auto cache = GetTypeInfoCache(as_engine);
    std::scoped_lock lock {cache->Locker};

    if (const auto it = cache->Map.find(type_key); it != cache->Map.end()) {
        return it->second;
    }

    const auto type_id = as_engine->GetTypeIdByDecl(type_key.c_str());
    FO_VERIFY_AND_THROW(type_id, "Missing AngelScript type id");
    nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoById(type_id);
    FO_VERIFY_AND_THROW(type_info, "Missing AngelScript type info");
    cache->Map.emplace(type_key, type_info);
    return type_info;
}

static auto LookupCachedTypeInfoForProperty(ptr<AngelScript::asIScriptEngine> as_engine, ptr<const Property> prop) -> ptr<AngelScript::asITypeInfo>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto cache = GetTypeInfoCache(as_engine);
    std::scoped_lock lock {cache->Locker};

    if (const auto it = cache->ByProperty.find(prop); it != cache->ByProperty.end()) {
        return it->second;
    }

    const auto type_name = MakeScriptPropertyName(prop);
    const auto type_id = as_engine->GetTypeIdByDecl(type_name.c_str());
    FO_VERIFY_AND_THROW(type_id, "Missing AngelScript type id");
    nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoById(type_id);
    FO_VERIFY_AND_THROW(type_info, "Missing AngelScript type info");
    cache->ByProperty.emplace(prop, type_info);
    cache->Map.try_emplace(type_name, type_info);
    return type_info;
}

auto CreateScriptArray(ptr<AngelScript::asIScriptEngine> as_engine, string_view type) -> refcount_ptr<ScriptArray>
{
    FO_STACK_TRACE_ENTRY();

    auto as_type_info = LookupCachedTypeInfo(as_engine, type);
    return ScriptArray::Create(as_type_info);
}

auto CreateScriptDict(ptr<AngelScript::asIScriptEngine> as_engine, string_view type) -> refcount_ptr<ScriptDict>
{
    FO_STACK_TRACE_ENTRY();

    auto as_type_info = LookupCachedTypeInfo(as_engine, type);
    return ScriptDict::Create(as_type_info);
}

auto CalcConstructAddrSpace(ptr<const Property> prop) -> size_t
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

void FreeConstructAddrSpace(ptr<const Property> prop, ptr<void> construct_addr)
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            cast_from_void<hstring*>(construct_addr.get())->~hstring();
        }
    }
    else if (prop->IsBaseTypeRefType()) {
        auto ref_obj = NativeDataProvider::ReadHandleSlot(construct_addr);

        if (ref_obj) {
            cast_from_void<DynamicRefTypeInstance*>(ref_obj.get())->Release();
        }
    }
    else if (prop->IsString()) {
        cast_from_void<string*>(construct_addr.get())->~string();
    }
    else if (prop->IsArray()) {
        if (auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(construct_addr)) {
            arr->Release();
        }
    }
    else if (prop->IsDict()) {
        if (auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(construct_addr)) {
            dict->Release();
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void CopyPropertyStructToScriptStruct(ptr<HashResolver> hash_resolver, const BaseTypeDesc& base_type, span<const uint8_t> raw_data, ptr<void> script_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout, "Struct layout is missing");
    FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw property struct size does not match the value type size", base_type.Name, raw_data.size(), base_type.Size);

    auto script_bytes = script_data.reinterpret_as<uint8_t>();

    for (const FieldDesc& field : base_type.StructLayout->Fields) {
        ptr<const uint8_t> field_raw = make_ptr(raw_data.data()).offset(field.Offset);
        ptr<uint8_t> field_script = script_bytes.offset(field.Offset);

        if (field.Type.IsHashedString) {
            hstring::hash_t hash {};
            MemCopy(&hash, field_raw, sizeof(hash));
            new (field_script.get()) hstring(hash != 0 ? hash_resolver->ResolveHash(hash) : hstring());
        }
        else if (field.Type.IsStruct && field.Type.StructLayout != nullptr) {
            CopyPropertyStructToScriptStruct(hash_resolver, field.Type, {field_raw.get(), field.Type.Size}, field_script);
        }
        else {
            MemCopy(field_script, field_raw, field.Type.Size);
        }
    }
}

static void CopyScriptStructToPropertyData(const BaseTypeDesc& base_type, ptr<const void> script_data, span<uint8_t> raw_data)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsStruct, "Base type is not a struct");
    FO_VERIFY_AND_THROW(base_type.StructLayout, "Struct layout is missing");
    FO_VERIFY_AND_THROW(raw_data.size() == base_type.Size, "Raw property struct size does not match the value type size", base_type.Name, raw_data.size(), base_type.Size);

    auto script_bytes = script_data.reinterpret_as<const uint8_t>();

    for (const FieldDesc& field : base_type.StructLayout->Fields) {
        ptr<const uint8_t> field_script = script_bytes.offset(field.Offset);
        ptr<uint8_t> field_raw = make_ptr(raw_data.data()).offset(field.Offset);

        if (field.Type.IsHashedString) {
            const hstring::hash_t hash = field_script.reinterpret_as<const hstring>()->as_hash();
            MemCopy(field_raw, &hash, sizeof(hash));
        }
        else if (field.Type.IsStruct && field.Type.StructLayout != nullptr) {
            CopyScriptStructToPropertyData(field.Type, field_script, {field_raw.get(), field.Type.Size});
        }
        else {
            MemCopy(field_raw, field_script, field.Type.Size);
        }
    }
}

void ConvertPropsToScriptObject(ptr<const Property> prop, PropertyRawData& prop_data, ptr<void> construct_addr, ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    const auto resolve_hash = [prop](const_span<uint8_t> hash_data) -> hstring {
        FO_VERIFY_AND_THROW(hash_data.size() == sizeof(hstring::hash_t), "Serialized hash payload size does not match hash storage size");
        const hstring::hash_t hash = MemReadUnaligned<hstring::hash_t>(hash_data.data());
        return hash ? prop->GetRegistrator()->GetHashResolver()->ResolveHash(hash) : hstring();
    };

    const auto resolve_fixed_type = [prop, as_engine, &resolve_hash](const_span<uint8_t> hash_data) -> nptr<Entity> {
        const hstring pid = resolve_hash(hash_data);

        if (!pid) {
            return nullptr;
        }

        auto engine = GetGameEngine(as_engine);
        const auto type_name = engine->Hashes.ToHashedString(prop->GetBaseTypeName());
        auto proto = engine->GetProtoEntity(type_name, pid);

        if (!proto) {
            throw ScriptException("Unable to resolve proto", prop->GetName(), pid);
        }

        return make_ptr(const_cast<ProtoEntity*>(std::addressof(*proto)));
    };

    const auto resolve_enum = [](const_span<uint8_t> enum_data) -> int32_t {
        int32_t result = 0;
        MemCopy(&result, enum_data.data(), enum_data.size());
        return result;
    };
    const auto create_ref_obj = [prop](const_span<uint8_t> ref_data) -> refcount_ptr<DynamicRefTypeInstance> { return CreateRefTypeScriptObjectFromProperty(prop, ref_data); };

    ptr<const uint8_t> data = prop_data.GetPtrAs<uint8_t>();
    const auto data_size = prop_data.GetSize();

    if (prop->IsPlainData()) {
        const auto data_span = make_const_span(data, data_size);
        size_t data_pos = 0;

        if (prop->IsBaseTypeProtoReference()) {
            FO_VERIFY_AND_THROW(data_size == sizeof(hstring::hash_t), "Serialized proto reference payload size does not match hash storage size", prop->GetName(), data_size, sizeof(hstring::hash_t));
            auto value_data = span_read_bytes(data_span, data_pos, data_size);
            new (construct_addr.get()) Entity*(resolve_fixed_type(value_data).get());
        }
        else if (prop->IsBaseTypeHash()) {
            FO_VERIFY_AND_THROW(data_size == sizeof(hstring::hash_t), "Serialized hash payload size does not match hash storage size", prop->GetName(), data_size, sizeof(hstring::hash_t));
            auto value_data = span_read_bytes(data_span, data_pos, data_size);
            new (construct_addr.get()) hstring(resolve_hash(value_data));
        }
        else if (prop->IsBaseTypeEnum()) {
            FO_VERIFY_AND_THROW(data_size != 0, "Serialized primitive payload has zero size", data_size);
            FO_VERIFY_AND_THROW(data_size <= sizeof(int32_t), "Serialized enum payload is wider than AngelScript integer storage", prop->GetName(), data_size, sizeof(int32_t));
            MemFill(construct_addr, 0, sizeof(int32_t));
            auto value_data = span_read_bytes(data_span, data_pos, data_size);
            MemCopy(construct_addr, value_data.data(), data_size);
        }
        else if (prop->IsBaseTypePrimitive()) {
            FO_VERIFY_AND_THROW(data_size != 0, "Serialized primitive payload has zero size", data_size);
            auto value_data = span_read_bytes(data_span, data_pos, data_size);
            MemCopy(construct_addr, value_data.data(), data_size);
        }
        else if (prop->IsBaseTypeStruct()) {
            FO_VERIFY_AND_THROW(data_size != 0, "Serialized primitive payload has zero size", data_size);
            auto value_data = span_read_bytes(data_span, data_pos, data_size);
            CopyPropertyStructToScriptStruct(prop->GetRegistrator()->GetHashResolver(), prop->GetBaseType(), value_data, construct_addr);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else if (prop->IsBaseTypeRefType() && !prop->IsArray() && !prop->IsDict()) {
        auto ref_obj = create_ref_obj(make_const_span(data, data_size));
        auto released_ref_obj = ref_obj.release_ownership();
        NativeDataProvider::WriteTypedHandleSlot<DynamicRefTypeInstance>(construct_addr, released_ref_obj);
    }
    else if (prop->IsString()) {
        const auto data_span = make_const_span(data, data_size);
        size_t data_pos = 0;
        new (construct_addr.get()) string(span_read_string(data_span, data_pos, data_size));
    }
    else if (prop->IsArray()) {
        auto prop_type_info = LookupCachedTypeInfoForProperty(as_engine, prop);
        auto arr = ScriptArray::Create(prop_type_info);

        if (prop->IsArrayOfString()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                    string str = span_read_string(data_span, data_pos, str_size);
                    arr->SetValue(numeric_cast<int32_t>(i), make_nptr(&str).void_cast());
                }
            }
        }
        else if (prop->IsBaseTypeRefType()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    uint32_t ref_data_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                    auto ref_data = span_read_aligned_bytes(data_span, data_pos, ref_data_size, MAX_SERIALIZED_ALIGNMENT);

                    auto ref_obj = create_ref_obj(ref_data);
                    arr->SetValue(numeric_cast<int32_t>(i), ref_obj.get_pp());
                }
            }
        }
        else if (prop->IsBaseTypeProtoReference()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto value_data = span_read_bytes(data_span, data_pos, sizeof(hstring::hash_t));
                    auto fixed_type = resolve_fixed_type(value_data);
                    arr->SetValue(numeric_cast<int32_t>(i), fixed_type.get_pp());
                }
            }
        }
        else if (prop->IsBaseTypeHash()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto value_data = span_read_bytes(data_span, data_pos, sizeof(hstring::hash_t));
                    const auto hvalue = resolve_hash(value_data);
                    arr->SetValue(numeric_cast<int32_t>(i), make_nptr(&hvalue).void_cast());
                }
            }
        }
        else if (prop->IsBaseTypeEnum()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                if (prop->GetBaseSize() == sizeof(int32_t)) {
                    auto values_data = span_read_bytes(data_span, data_pos, data_size);
                    MemCopy(arr->At(0), values_data.data(), data_size);
                }
                else {
                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto value_data = span_read_bytes(data_span, data_pos, prop->GetBaseSize());
                        MemCopy(arr->At(numeric_cast<int32_t>(i)), value_data.data(), prop->GetBaseSize());
                    }
                }
            }
        }
        else if (prop->IsBaseTypePrimitive()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));
                auto values_data = span_read_bytes(data_span, data_pos, data_size);
                MemCopy(arr->At(0), values_data.data(), data_size);
            }
        }
        else if (prop->IsBaseTypeStruct()) {
            if (data_size != 0) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto arr_size = numeric_cast<uint32_t>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32_t>(arr_size));

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto value_data = span_read_bytes(data_span, data_pos, prop->GetBaseSize());
                    CopyPropertyStructToScriptStruct(prop->GetRegistrator()->GetHashResolver(), prop->GetBaseType(), value_data, arr->At(numeric_cast<int32_t>(i)));
                }
            }
        }
        else {
            FO_UNREACHABLE_PLACE();
        }

        NativeDataProvider::WriteTypedHandleSlot<ScriptArray>(construct_addr, arr);
        (void)arr.release_ownership();
    }
    else if (prop->IsDict()) {
        auto prop_type_info = LookupCachedTypeInfoForProperty(as_engine, prop);
        auto dict = ScriptDict::Create(prop_type_info);

        if (data_size != 0) {
            if (prop->IsDictOfArray()) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;
                const auto inner_array_type_name = strex("array<{}{}>", prop->GetBaseTypeName(), prop->IsBaseTypeRefType() ? "@" : "").str();
                auto inner_array_type_info = LookupCachedTypeInfo(as_engine, inner_array_type_name.c_str());

                const auto read_array = [&](uint32_t arr_size) -> refcount_ptr<ScriptArray> {
                    auto arr = ScriptArray::Create(inner_array_type_info);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                const uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                                string str = span_read_string(data_span, data_pos, str_size);
                                arr->SetValue(numeric_cast<int32_t>(i), make_nptr(&str).void_cast());
                            }
                        }
                        else if (prop->IsBaseTypeRefType()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                const uint32_t ref_data_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                                auto ref_data = span_read_aligned_bytes(data_span, data_pos, ref_data_size, MAX_SERIALIZED_ALIGNMENT);
                                auto ref_obj = create_ref_obj(ref_data);
                                arr->SetValue(numeric_cast<int32_t>(i), ref_obj.get_pp());
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto value_data = span_read_aligned_bytes(data_span, data_pos, sizeof(hstring::hash_t), alignment_for_size(sizeof(hstring::hash_t)));
                                auto fixed_type = resolve_fixed_type(value_data);
                                arr->SetValue(numeric_cast<int32_t>(i), fixed_type.get_pp());
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto value_data = span_read_aligned_bytes(data_span, data_pos, sizeof(hstring::hash_t), alignment_for_size(sizeof(hstring::hash_t)));
                                const auto hvalue = resolve_hash(value_data);
                                arr->SetValue(numeric_cast<int32_t>(i), make_nptr(&hvalue).void_cast());
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            const size_t values_size = arr_size * prop->GetBaseSize();

                            if (prop->GetBaseSize() == sizeof(int32_t)) {
                                auto values_data = span_read_aligned_bytes(data_span, data_pos, values_size, alignment_for_size(prop->GetBaseSize()));
                                MemCopy(arr->At(0), values_data.data(), values_size);
                            }
                            else {
                                for (uint32_t i = 0; i < arr_size; i++) {
                                    auto value_data = span_read_aligned_bytes(data_span, data_pos, prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                                    MemCopy(arr->At(numeric_cast<int32_t>(i)), value_data.data(), prop->GetBaseSize());
                                }
                            }
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            const size_t values_size = arr_size * prop->GetBaseSize();
                            auto values_data = span_read_aligned_bytes(data_span, data_pos, values_size, alignment_for_size(prop->GetBaseSize()));
                            MemCopy(arr->At(0), values_data.data(), values_size);
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            arr->Resize(numeric_cast<int32_t>(arr_size));

                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto value_data = span_read_aligned_bytes(data_span, data_pos, prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                                MemCopy(arr->At(numeric_cast<int32_t>(i)), value_data.data(), prop->GetBaseSize());
                            }
                        }
                        else {
                            FO_UNREACHABLE_PLACE();
                        }
                    }

                    return arr;
                };

                while (data_pos < data_span.size()) {
                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        const string key_str = span_read_string(data_span, data_pos, key_len);
                        const uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        auto arr = read_array(arr_size);
                        dict->Set(make_nptr(&key_str).void_cast(), make_ptr(arr.get_pp()).void_cast());
                        continue;
                    }

                    auto key = span_read_aligned_bytes(data_span, data_pos, prop->GetDictKeyTypeSize(), alignment_for_size(prop->GetDictKeyTypeSize()));
                    const uint32_t arr_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                    auto arr = read_array(arr_size);

                    if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(make_nptr(&hkey).void_cast(), make_ptr(arr.get_pp()).void_cast());
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const int32_t ekey = resolve_enum(key);
                        dict->Set(make_nptr(&ekey).void_cast(), make_ptr(arr.get_pp()).void_cast());
                    }
                    else {
                        dict->Set(make_nptr(key.data()).void_cast(), make_ptr(arr.get_pp()).void_cast());
                    }
                }
            }
            else if (prop->IsDictOfString()) {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;

                while (data_pos < data_span.size()) {
                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        const string key_str = span_read_string(data_span, data_pos, key_len);
                        const uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        string str = span_read_string(data_span, data_pos, str_size);
                        dict->Set(make_nptr(&key_str).void_cast(), make_nptr(&str).void_cast());
                        continue;
                    }

                    auto key = span_read_aligned_bytes(data_span, data_pos, prop->GetDictKeyTypeSize(), alignment_for_size(prop->GetDictKeyTypeSize()));
                    const uint32_t str_size = span_read_aligned_object<uint32_t>(data_span, data_pos);
                    string str = span_read_string(data_span, data_pos, str_size);

                    if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(make_nptr(&hkey).void_cast(), make_nptr(&str).void_cast());
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const int32_t ekey = resolve_enum(key);
                        dict->Set(make_nptr(&ekey).void_cast(), make_nptr(&str).void_cast());
                    }
                    else {
                        dict->Set(make_nptr(key.data()).void_cast(), make_nptr(&str).void_cast());
                    }
                }
            }
            else {
                const auto data_span = make_const_span(data, data_size);
                size_t data_pos = 0;

                const auto read_ref_obj = [&]() -> refcount_ptr<DynamicRefTypeInstance> {
                    const uint32_t ref_data_size = span_read_aligned_object<uint32_t>(data_span, data_pos);

                    auto ref_data = span_read_aligned_bytes(data_span, data_pos, ref_data_size, MAX_SERIALIZED_ALIGNMENT);
                    return create_ref_obj(ref_data);
                };
                const auto read_fixed_type = [&]() -> nptr<Entity> {
                    auto value_data = span_read_aligned_bytes(data_span, data_pos, prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                    return resolve_fixed_type(value_data);
                };
                const auto read_hash = [&]() -> hstring {
                    auto value_data = span_read_aligned_bytes(data_span, data_pos, prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                    return resolve_hash(value_data);
                };
                const auto read_raw_value = [&]() -> const_span<uint8_t> { return span_read_aligned_bytes(data_span, data_pos, prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize())); };

                while (data_pos < data_span.size()) {
                    if (prop->IsDictKeyString()) {
                        const uint32_t key_len = span_read_aligned_object<uint32_t>(data_span, data_pos);
                        const string key_str = span_read_string(data_span, data_pos, key_len);

                        if (prop->IsBaseTypeRefType()) {
                            auto ref_obj = read_ref_obj();
                            dict->Set(make_nptr(&key_str).void_cast(), make_ptr(ref_obj.get_pp()).void_cast());
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            auto fixed_type = read_fixed_type();
                            dict->Set(make_nptr(&key_str).void_cast(), make_ptr(fixed_type.get_pp()).void_cast());
                        }
                        else if (prop->IsBaseTypeHash()) {
                            const auto hvalue = read_hash();
                            dict->Set(make_nptr(&key_str).void_cast(), make_nptr(&hvalue).void_cast());
                        }
                        else {
                            const_span<uint8_t> value_data = read_raw_value();
                            dict->Set(make_nptr(&key_str).void_cast(), make_nptr(value_data.data()).void_cast());
                        }
                    }
                    else {
                        auto key = span_read_aligned_bytes(data_span, data_pos, prop->GetDictKeyTypeSize(), alignment_for_size(prop->GetDictKeyTypeSize()));

                        if (prop->IsDictKeyHash()) {
                            const auto hkey = resolve_hash(key);

                            if (prop->IsBaseTypeRefType()) {
                                auto ref_obj = read_ref_obj();
                                dict->Set(make_nptr(&hkey).void_cast(), make_ptr(ref_obj.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeProtoReference()) {
                                auto fixed_type = read_fixed_type();
                                dict->Set(make_nptr(&hkey).void_cast(), make_ptr(fixed_type.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeHash()) {
                                const auto hvalue = read_hash();
                                dict->Set(make_nptr(&hkey).void_cast(), make_nptr(&hvalue).void_cast());
                            }
                            else {
                                const_span<uint8_t> value_data = read_raw_value();
                                dict->Set(make_nptr(&hkey).void_cast(), make_nptr(value_data.data()).void_cast());
                            }
                        }
                        else if (prop->IsDictKeyEnum()) {
                            const int32_t ekey = resolve_enum(key);

                            if (prop->IsBaseTypeRefType()) {
                                auto ref_obj = read_ref_obj();
                                dict->Set(make_nptr(&ekey).void_cast(), make_ptr(ref_obj.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeProtoReference()) {
                                auto fixed_type = read_fixed_type();
                                dict->Set(make_nptr(&ekey).void_cast(), make_ptr(fixed_type.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeHash()) {
                                const auto hvalue = read_hash();
                                dict->Set(make_nptr(&ekey).void_cast(), make_nptr(&hvalue).void_cast());
                            }
                            else {
                                const_span<uint8_t> value_data = read_raw_value();
                                dict->Set(make_nptr(&ekey).void_cast(), make_nptr(value_data.data()).void_cast());
                            }
                        }
                        else {
                            if (prop->IsBaseTypeRefType()) {
                                auto ref_obj = read_ref_obj();
                                dict->Set(make_nptr(key.data()).void_cast(), make_ptr(ref_obj.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeProtoReference()) {
                                auto fixed_type = read_fixed_type();
                                dict->Set(make_nptr(key.data()).void_cast(), make_ptr(fixed_type.get_pp()).void_cast());
                            }
                            else if (prop->IsBaseTypeHash()) {
                                const auto hvalue = read_hash();
                                dict->Set(make_nptr(key.data()).void_cast(), make_nptr(&hvalue).void_cast());
                            }
                            else {
                                const_span<uint8_t> value_data = read_raw_value();
                                dict->Set(make_nptr(key.data()).void_cast(), make_nptr(value_data.data()).void_cast());
                            }
                        }
                    }
                }
            }
        }

        NativeDataProvider::WriteTypedHandleSlot<ScriptDict>(construct_addr, dict);
        (void)dict.release_ownership();
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

auto ConvertScriptToPropsObject(ptr<const Property> prop, ptr<void> as_obj) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    PropertyRawData prop_data;

    const auto resolve_proto_hash = [](nptr<const Entity> entity) -> hstring::hash_t {
        if (entity) {
            auto proto_entity = entity.dyn_cast<const ProtoEntity>();
            FO_VERIFY_AND_THROW(proto_entity, "Missing required prototype entity");
            return proto_entity->GetProtoId().as_hash();
        }
        else {
            return {};
        }
    };

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeProtoReference()) {
            auto entity = NativeDataProvider::ReadTypedHandleSlot<Entity>(as_obj);
            const auto pid = resolve_proto_hash(entity);
            FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(pid), "Property base size does not match proto id storage size", prop->GetBaseSize(), sizeof(pid));
            prop_data.SetAs<hstring::hash_t>(pid);
        }
        else if (prop->IsBaseTypeHash()) {
            const auto hash = as_obj.reinterpret_as<const hstring>()->as_hash();
            FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hash), "Property base size does not match hash storage size", prop->GetBaseSize(), sizeof(hash));
            prop_data.SetAs<hstring::hash_t>(hash);
        }
        else if (prop->IsBaseTypeStruct()) {
            ptr<uint8_t> raw_data = prop_data.Alloc(prop->GetBaseSize());
            CopyScriptStructToPropertyData(prop->GetBaseType(), as_obj, {raw_data.get(), prop->GetBaseSize()});
        }
        else {
            prop_data.Set(as_obj.get(), prop->GetBaseSize());
        }
    }
    else if (prop->IsBaseTypeRefType() && !prop->IsArray() && !prop->IsDict()) {
        const auto ref_obj = NativeDataProvider::ReadHandleSlot(as_obj);

        if (ref_obj) {
            prop_data = ConvertRefTypeScriptObjectToProperty(prop, ref_obj);
        }
    }
    else if (prop->IsString()) {
        auto str = as_obj.reinterpret_as<const string>();
        prop_data.Pass(str->data(), str->length());
    }
    else if (prop->IsArray()) {
        auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(as_obj);

        if (prop->IsArrayOfString()) {
            uint32_t arr_size = 0;

            if (arr) {
                arr_size = numeric_cast<uint32_t>(arr->GetSize());
            }

            if (arr_size != 0) {
                // Calculate size
                size_t data_size = sizeof(uint32_t);

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto str = arr->AtAs<const string>(i);
                    data_size = align_up(data_size, sizeof(uint32_t));
                    data_size += sizeof(uint32_t) + numeric_cast<uint32_t>(str->length());
                }

                // Make buffer
                auto buf = prop_data.Alloc(data_size);
                MemFill(buf, 0, data_size);
                auto buf_span = make_span(buf, data_size);
                size_t data_pos = 0;

                span_write_aligned_object(buf_span, data_pos, arr_size);

                for (uint32_t i = 0; i < arr_size; i++) {
                    auto str = arr->AtAs<const string>(i);

                    const auto str_size = numeric_cast<uint32_t>(str->length());
                    span_write_aligned_object(buf_span, data_pos, str_size);
                    span_write_string(buf_span, data_pos, *str);
                }

                FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
            }
        }
        else {
            uint32_t arr_size = 0;

            if (arr) {
                arr_size = numeric_cast<uint32_t>(arr->GetSize());
            }

            const size_t data_size = [&]() -> size_t {
                if (arr_size == 0) {
                    return 0;
                }

                if (prop->IsBaseTypeRefType()) {
                    size_t size = sizeof(uint32_t);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, arr->AtAs<void>(i));
                        size = align_up(size, sizeof(uint32_t));
                        size += sizeof(uint32_t);

                        if (ref_data.GetSize() != 0) {
                            size = align_up(size, MAX_SERIALIZED_ALIGNMENT);
                            size += ref_data.GetSize();
                        }
                    }

                    return size;
                }

                return numeric_cast<size_t>(arr_size * prop->GetBaseSize());
            }();

            if (data_size != 0) {
                if (prop->IsBaseTypeRefType()) {
                    auto buf = prop_data.Alloc(data_size);
                    MemFill(buf, 0, data_size);
                    auto buf_span = make_span(buf, data_size);
                    size_t data_pos = 0;

                    span_write_aligned_object(buf_span, data_pos, arr_size);

                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, arr->AtAs<void>(i));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        span_write_aligned_object(buf_span, data_pos, ref_data_size);

                        span_write_aligned_bytes(buf_span, data_pos, ref_data.GetPtr(), ref_data.GetSize(), MAX_SERIALIZED_ALIGNMENT);
                    }

                    FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
                }
                else if (prop->IsBaseTypeProtoReference()) {
                    auto buf = prop_data.Alloc(data_size);
                    auto buf_span = make_span(buf, data_size);
                    size_t data_pos = 0;

                    for (uint32_t i = 0; i < arr_size; i++) {
                        auto entity = arr->AtAs<Entity>(i);
                        const auto pid = resolve_proto_hash(entity);
                        FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(pid), "Property base size does not match proto id storage size");
                        span_write_aligned_object(buf_span, data_pos, pid);
                    }

                    FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
                }
                else if (prop->IsBaseTypeHash()) {
                    auto buf = prop_data.Alloc(data_size);
                    auto buf_span = make_span(buf, data_size);
                    size_t data_pos = 0;

                    for (uint32_t i = 0; i < arr_size; i++) {
                        const hstring::hash_t hash = arr->AtAs<const hstring>(i)->as_hash();
                        FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hash), "Property base size does not match hash storage size");
                        span_write_aligned_object(buf_span, data_pos, hash);
                    }

                    FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
                }
                else if (prop->IsBaseTypeEnum()) {
                    if (prop->GetBaseSize() == sizeof(int32_t)) {
                        prop_data.Pass(arr->At(0), data_size);
                    }
                    else {
                        auto buf = prop_data.Alloc(data_size);
                        auto buf_span = make_span(buf, data_size);
                        size_t data_pos = 0;

                        for (uint32_t i = 0; i < arr_size; i++) {
                            span_write_bytes(buf_span, data_pos, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                        }

                        FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
                    }
                }
                else if (prop->IsBaseTypePrimitive()) {
                    prop_data.Pass(arr->At(0), data_size);
                }
                else if (prop->IsBaseTypeStruct()) {
                    auto buf = prop_data.Alloc(data_size);
                    auto buf_span = make_span(buf, data_size);
                    size_t data_pos = 0;

                    for (uint32_t i = 0; i < arr_size; i++) {
                        span_write_bytes(buf_span, data_pos, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize());
                    }

                    FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
                }
                else {
                    FO_UNREACHABLE_PLACE();
                }
            }
        }
    }
    else if (prop->IsDict()) {
        auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(as_obj);

        if (prop->IsDictOfArray()) {
            if (dict && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    if (prop->IsDictKeyString()) {
                        auto key_str = dict->KeyAs<const string>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str->length());
                        data_size = align_up(data_size, sizeof(uint32_t));
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size = align_up(data_size, alignment_for_size(prop->GetDictKeyTypeSize()));
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    auto arr = dict->ValueAs<ScriptArray>(value);
                    uint32_t arr_size = 0;

                    if (arr) {
                        arr_size = numeric_cast<uint32_t>(arr->GetSize());
                    }

                    data_size = align_up(data_size, sizeof(uint32_t));
                    data_size += sizeof(arr_size);

                    if (prop->IsDictOfArrayOfString()) {
                        if (arr_size == 0) {
                            continue;
                        }

                        for (uint32_t i = 0; i < arr_size; i++) {
                            auto str = arr->AtAs<const string>(i);
                            data_size = align_up(data_size, sizeof(uint32_t));
                            data_size += sizeof(uint32_t) + numeric_cast<uint32_t>(str->length());
                        }
                    }
                    else if (prop->IsBaseTypeRefType()) {
                        if (arr_size == 0) {
                            continue;
                        }

                        for (uint32_t i = 0; i < arr_size; i++) {
                            auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, arr->AtAs<void>(i));
                            data_size = align_up(data_size, sizeof(uint32_t));
                            data_size += sizeof(uint32_t);

                            if (ref_data.GetSize() != 0) {
                                data_size = align_up(data_size, MAX_SERIALIZED_ALIGNMENT);
                                data_size += ref_data.GetSize();
                            }
                        }
                    }
                    else {
                        if (arr_size != 0) {
                            data_size = align_up(data_size, alignment_for_size(prop->GetBaseSize()));
                            data_size += arr_size * prop->GetBaseSize();
                        }
                    }
                }

                // Make buffer
                auto buf = prop_data.Alloc(data_size);
                MemFill(buf, 0, data_size);
                auto buf_span = make_span(buf, data_size);
                size_t data_pos = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    auto arr = dict->ValueAs<ScriptArray>(value);

                    if (prop->IsDictKeyString()) {
                        auto key_str = dict->KeyAs<const string>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str->length());
                        span_write_aligned_object(buf_span, data_pos, key_len);
                        span_write_string(buf_span, data_pos, *key_str);
                    }
                    else if (prop->IsDictKeyHash()) {
                        const hstring::hash_t hkey = dict->KeyAs<const hstring>(key)->as_hash();
                        span_write_aligned_object_bytes(buf_span, data_pos, hkey, prop->GetDictKeyTypeSize());
                    }
                    else {
                        span_write_aligned_bytes(buf_span, data_pos, dict->KeyAs<const void>(key), prop->GetDictKeyTypeSize(), alignment_for_size(prop->GetDictKeyTypeSize()));
                    }

                    uint32_t arr_size = 0;

                    if (arr) {
                        arr_size = numeric_cast<uint32_t>(arr->GetSize());
                    }

                    span_write_aligned_object(buf_span, data_pos, arr_size);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto str = arr->AtAs<const string>(i);
                                const auto str_size = numeric_cast<uint32_t>(str->length());

                                span_write_aligned_object(buf_span, data_pos, str_size);
                                span_write_string(buf_span, data_pos, *str);
                            }
                        }
                        else if (prop->IsBaseTypeRefType()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, arr->AtAs<void>(i));
                                const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                                span_write_aligned_object(buf_span, data_pos, ref_data_size);

                                span_write_aligned_bytes(buf_span, data_pos, ref_data.GetPtr(), ref_data.GetSize(), MAX_SERIALIZED_ALIGNMENT);
                            }
                        }
                        else if (prop->IsBaseTypeProtoReference()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                auto entity = arr->AtAs<Entity>(i);
                                const auto pid = resolve_proto_hash(entity);
                                FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(pid), "Property base size does not match proto id storage size");
                                span_write_aligned_object(buf_span, data_pos, pid);
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                const hstring::hash_t hash = arr->AtAs<const hstring>(i)->as_hash();
                                FO_VERIFY_AND_THROW(prop->GetBaseSize() == sizeof(hash), "Property base size does not match hash storage size");
                                span_write_aligned_object(buf_span, data_pos, hash);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            if (prop->GetBaseSize() == sizeof(int32_t)) {
                                span_write_aligned_bytes(buf_span, data_pos, arr->At(0), arr_size * prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                            }
                            else {
                                for (uint32_t i = 0; i < arr_size; i++) {
                                    span_write_aligned_bytes(buf_span, data_pos, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                                }
                            }
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            span_write_aligned_bytes(buf_span, data_pos, arr->At(0), arr_size * prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            for (uint32_t i = 0; i < arr_size; i++) {
                                span_write_aligned_bytes(buf_span, data_pos, arr->At(numeric_cast<int32_t>(i)), prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                            }
                        }
                        else {
                            FO_UNREACHABLE_PLACE();
                        }
                    }
                }

                FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
            }
        }
        else if (prop->IsDictOfString()) {
            if (dict && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    if (prop->IsDictKeyString()) {
                        auto key_str = dict->KeyAs<const string>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str->length());
                        data_size = align_up(data_size, sizeof(uint32_t));
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size = align_up(data_size, alignment_for_size(prop->GetDictKeyTypeSize()));
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    auto str = dict->ValueAs<const string>(value);
                    const auto str_size = numeric_cast<uint32_t>(str->length());

                    data_size = align_up(data_size, sizeof(uint32_t));
                    data_size += sizeof(str_size) + str_size;
                }

                // Make buffer
                auto buf = prop_data.Alloc(data_size);
                MemFill(buf, 0, data_size);
                auto buf_span = make_span(buf, data_size);
                size_t data_pos = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    auto str = dict->ValueAs<const string>(value);

                    if (prop->IsDictKeyString()) {
                        auto key_str = dict->KeyAs<const string>(key);
                        const auto key_len = numeric_cast<uint32_t>(key_str->length());
                        span_write_aligned_object(buf_span, data_pos, key_len);
                        span_write_string(buf_span, data_pos, *key_str);
                    }
                    else if (prop->IsDictKeyHash()) {
                        const hstring::hash_t hkey = dict->KeyAs<const hstring>(key)->as_hash();
                        span_write_aligned_object_bytes(buf_span, data_pos, hkey, prop->GetDictKeyTypeSize());
                    }
                    else {
                        span_write_aligned_bytes(buf_span, data_pos, dict->KeyAs<const void>(key), prop->GetDictKeyTypeSize(), alignment_for_size(prop->GetDictKeyTypeSize()));
                    }

                    const auto str_size = numeric_cast<uint32_t>(str->length());
                    span_write_aligned_object(buf_span, data_pos, str_size);
                    span_write_string(buf_span, data_pos, *str);
                }

                FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
            }
        }
        else if (prop->IsDictKeyString()) {
            if (dict && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    auto key_str = dict->KeyAs<const string>(key);
                    const auto key_len = numeric_cast<uint32_t>(key_str->length());
                    data_size = align_up(data_size, sizeof(uint32_t));
                    data_size += sizeof(key_len) + key_len;

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, dict->ValueAs<void>(value));
                        data_size = align_up(data_size, sizeof(uint32_t));
                        data_size += sizeof(uint32_t);

                        if (ref_data.GetSize() != 0) {
                            data_size = align_up(data_size, MAX_SERIALIZED_ALIGNMENT);
                            data_size += ref_data.GetSize();
                        }
                    }
                    else {
                        data_size = align_up(data_size, alignment_for_size(prop->GetBaseSize()));
                        data_size += prop->GetBaseSize();
                    }
                }

                // Make buffer
                auto buf = prop_data.Alloc(data_size);
                MemFill(buf, 0, data_size);
                auto buf_span = make_span(buf, data_size);
                size_t data_pos = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    auto key_str = dict->KeyAs<const string>(key);
                    const auto key_len = numeric_cast<uint32_t>(key_str->length());

                    span_write_aligned_object(buf_span, data_pos, key_len);
                    span_write_string(buf_span, data_pos, *key_str);

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, dict->ValueAs<void>(value));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        span_write_aligned_object(buf_span, data_pos, ref_data_size);

                        span_write_aligned_bytes(buf_span, data_pos, ref_data.GetPtr(), ref_data.GetSize(), MAX_SERIALIZED_ALIGNMENT);
                    }
                    else if (prop->IsBaseTypeProtoReference()) {
                        auto entity = dict->ValueAs<Entity>(value);
                        const auto pid = resolve_proto_hash(entity);
                        span_write_aligned_object_bytes(buf_span, data_pos, pid, prop->GetBaseSize());
                    }
                    else if (prop->IsBaseTypeHash()) {
                        const hstring::hash_t hash = dict->ValueAs<const hstring>(value)->as_hash();
                        span_write_aligned_object_bytes(buf_span, data_pos, hash, prop->GetBaseSize());
                    }
                    else {
                        span_write_aligned_bytes(buf_span, data_pos, dict->ValueAs<const void>(value), prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                    }
                }

                FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeyTypeSize();
            const auto data_size = [&]() {
                if (!dict || dict->GetSize() == 0) {
                    return size_t {};
                }

                size_t size = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    ignore_unused(key);
                    size = align_up(size, alignment_for_size(key_element_size));
                    size += key_element_size;

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, dict->ValueAs<void>(value));
                        size = align_up(size, sizeof(uint32_t));
                        size += sizeof(uint32_t);

                        if (ref_data.GetSize() != 0) {
                            size = align_up(size, MAX_SERIALIZED_ALIGNMENT);
                            size += ref_data.GetSize();
                        }
                    }
                    else {
                        size = align_up(size, alignment_for_size(prop->GetBaseSize()));
                        size += prop->GetBaseSize();
                    }
                }

                return size;
            }();

            if (data_size != 0) {
                auto buf = prop_data.Alloc(data_size);
                MemFill(buf, 0, data_size);
                auto buf_span = make_span(buf, data_size);
                size_t data_pos = 0;

                for (auto&& [key, value] : *dict->GetMap()) {
                    if (prop->IsDictKeyHash()) {
                        const hstring::hash_t hkey = dict->KeyAs<const hstring>(key)->as_hash();
                        span_write_aligned_object_bytes(buf_span, data_pos, hkey, key_element_size);
                    }
                    else {
                        span_write_aligned_bytes(buf_span, data_pos, dict->KeyAs<const void>(key), key_element_size, alignment_for_size(key_element_size));
                    }

                    if (prop->IsBaseTypeRefType()) {
                        auto ref_data = ConvertRefTypeScriptObjectToProperty(prop, dict->ValueAs<void>(value));
                        const auto ref_data_size = numeric_cast<uint32_t>(ref_data.GetSize());
                        span_write_aligned_object(buf_span, data_pos, ref_data_size);

                        span_write_aligned_bytes(buf_span, data_pos, ref_data.GetPtr(), ref_data.GetSize(), MAX_SERIALIZED_ALIGNMENT);
                    }
                    else if (prop->IsBaseTypeProtoReference()) {
                        auto entity = dict->ValueAs<Entity>(value);
                        const auto pid = resolve_proto_hash(entity);
                        span_write_aligned_object_bytes(buf_span, data_pos, pid, prop->GetBaseSize());
                    }
                    else if (prop->IsBaseTypeHash()) {
                        const hstring::hash_t hash = dict->ValueAs<const hstring>(value)->as_hash();
                        span_write_aligned_object_bytes(buf_span, data_pos, hash, prop->GetBaseSize());
                    }
                    else {
                        span_write_aligned_bytes(buf_span, data_pos, dict->ValueAs<const void>(value), prop->GetBaseSize(), alignment_for_size(prop->GetBaseSize()));
                    }
                }

                FO_VERIFY_AND_THROW(data_pos == data_size, "Serialized property byte count does not match buffer size");
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    return prop_data;
}

auto GetScriptObjectInfo(ptr<const void> script_obj, int32_t type_id) -> string
{
    FO_STACK_TRACE_ENTRY();

    switch (type_id) {
    case AngelScript::asTYPEID_VOID:
        return "void";
    case AngelScript::asTYPEID_BOOL:
        return strex("bool: {}", *script_obj.reinterpret_as<bool>() ? "true" : "false");
    case AngelScript::asTYPEID_INT8:
        return strex("int8: {}", *script_obj.reinterpret_as<int8_t>());
    case AngelScript::asTYPEID_INT16:
        return strex("int16: {}", *script_obj.reinterpret_as<int16_t>());
    case AngelScript::asTYPEID_INT32:
        return strex("int32: {}", *script_obj.reinterpret_as<int32_t>());
    case AngelScript::asTYPEID_INT64:
        return strex("int64: {}", *script_obj.reinterpret_as<int64_t>());
    case AngelScript::asTYPEID_UINT8:
        return strex("uint8: {}", *script_obj.reinterpret_as<uint8_t>());
    case AngelScript::asTYPEID_UINT16:
        return strex("uint16: {}", *script_obj.reinterpret_as<uint16_t>());
    case AngelScript::asTYPEID_UINT32:
        return strex("uint32: {}", *script_obj.reinterpret_as<uint32_t>());
    case AngelScript::asTYPEID_UINT64:
        return strex("uint64: {}", *script_obj.reinterpret_as<uint64_t>());
    case AngelScript::asTYPEID_FLOAT:
        return strex("float32: {}", *script_obj.reinterpret_as<float32_t>());
    case AngelScript::asTYPEID_DOUBLE:
        return strex("float64: {}", *script_obj.reinterpret_as<float64_t>());
    default:
        break;
    }

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
    nptr<const AngelScript::asITypeInfo> as_type_info = as_engine->GetTypeInfoById(type_id);
    FO_VERIFY_AND_THROW(as_type_info, "Missing required AngelScript type info");
    const string type_name = as_type_info->GetName();
    auto meta = GetEngineMetadata(as_engine);

    if (type_name == "string") {
        auto value = script_obj.reinterpret_as<const string>();
        return strex("string: {}", *value);
    }
    if (type_name == "hstring") {
        auto value = script_obj.reinterpret_as<const hstring>();
        return strex("hstring: {}", *value);
    }
    if (type_name == "any") {
        return strex("any: {}", *script_obj.reinterpret_as<any_t>());
    }
    if (type_name == "ident") {
        return strex("ident: {}", *script_obj.reinterpret_as<ident_t>());
    }
    if (type_name == "timespan") {
        return strex("timespan: {}", *script_obj.reinterpret_as<timespan>());
    }
    if (type_name == "nanotime") {
        return strex("nanotime: {}", *script_obj.reinterpret_as<nanotime>());
    }
    if (type_name == "synctime") {
        return strex("synctime: {}", *script_obj.reinterpret_as<synctime>());
    }

    if (const auto enum_value_count = as_type_info->GetEnumValueCount(); enum_value_count != 0) {
        int32_t enum_value = 0;

        if (meta->IsValidBaseType(type_name)) {
            const auto& enum_type = meta->GetBaseType(type_name);
            enum_value = ReadEnumValueAsInt32(script_obj, enum_type);

            bool failed = false;
            const string_view enum_value_name = meta->ResolveEnumValueName(type_name, enum_value, &failed);

            if (!failed) {
                return strex("{}: {}", type_name, enum_value_name);
            }
        }
        else {
            enum_value = *script_obj.reinterpret_as<int32_t>();
        }

        for (AngelScript::asUINT i = 0; i < enum_value_count; i++) {
            AngelScript::asINT64 check_enum_value = 0;
            ptr<const char> enum_value_name = as_type_info->GetEnumValueByIndex(i, &check_enum_value);

            if (check_enum_value == enum_value) {
                return strex("{}: {}", type_name, enum_value_name.get());
            }
        }

        return strex("{}: {}", type_name, enum_value);
    }

    return strex("{}", type_name);
}

auto GetScriptFuncName(ptr<const AngelScript::asIScriptFunction> func, HashResolver& hash_resolver) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    const nptr<const char> ns = func->GetNamespace();
    const nptr<const char> name = func->GetName();
    const string_view ns_view = ns ? string_view {ns.get()} : string_view {};
    const string_view name_view = name ? string_view {name.get()} : string_view {};

    string func_name;

    if (ns_view.empty()) {
        func_name = string(name_view);
    }
    else {
        func_name = strex("{}::{}", ns_view, name_view).str();
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

auto ReadEnumValueAsInt32(ptr<const void> ptr, const BaseTypeDesc& enum_type) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(enum_type.IsEnum, "Type is not an enum");
    FO_VERIFY_AND_THROW(enum_type.EnumUnderlyingType, "Enum underlying type is null");
    FO_VERIFY_AND_THROW(enum_type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");

    if (enum_type.EnumUnderlyingType->IsSignedInt) {
        switch (enum_type.Size) {
        case sizeof(int8_t):
            return *ptr.reinterpret_as<int8_t>();
        case sizeof(int16_t):
            return *ptr.reinterpret_as<int16_t>();
        case sizeof(int32_t):
            return *ptr.reinterpret_as<int32_t>();
        default:
            break;
        }
    }
    else {
        switch (enum_type.Size) {
        case sizeof(uint8_t):
            return *ptr.reinterpret_as<uint8_t>();
        case sizeof(uint16_t):
            return *ptr.reinterpret_as<uint16_t>();
        case sizeof(uint32_t): {
            const uint32_t uint_value = *ptr.reinterpret_as<uint32_t>();
            return static_cast<int32_t>(uint_value);
        }
        default:
            break;
        }
    }

    throw GenericException("Unsupported enum underlying size", enum_type.Name, enum_type.Size);
}

void WriteEnumValueFromInt32(ptr<void> ptr, const BaseTypeDesc& enum_type, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(enum_type.IsEnum, "Type is not an enum");
    FO_VERIFY_AND_THROW(enum_type.EnumUnderlyingType, "Enum underlying type is null");
    FO_VERIFY_AND_THROW(enum_type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");

    if (enum_type.EnumUnderlyingType->IsSignedInt) {
        switch (enum_type.Size) {
        case sizeof(int8_t):
            *ptr.reinterpret_as<int8_t>() = numeric_cast<int8_t>(value);
            return;
        case sizeof(int16_t):
            *ptr.reinterpret_as<int16_t>() = numeric_cast<int16_t>(value);
            return;
        case sizeof(int32_t):
            *ptr.reinterpret_as<int32_t>() = value;
            return;
        default:
            break;
        }
    }
    else {
        switch (enum_type.Size) {
        case sizeof(uint8_t):
            *ptr.reinterpret_as<uint8_t>() = numeric_cast<uint8_t>(value);
            return;
        case sizeof(uint16_t):
            *ptr.reinterpret_as<uint16_t>() = numeric_cast<uint16_t>(value);
            return;
        case sizeof(uint32_t):
            *ptr.reinterpret_as<uint32_t>() = static_cast<uint32_t>(value);
            return;
        default:
            break;
        }
    }

    throw GenericException("Unsupported enum underlying size", enum_type.Name, enum_type.Size);
}

auto CreateRefTypeScriptObjectFromRawData(const BaseTypeDesc& base_type, span<const uint8_t> raw_data) -> refcount_ptr<DynamicRefTypeInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");

    auto ref_instance = SafeAlloc::MakeRefCounted<DynamicRefTypeInstance>(base_type.RefType->FieldsRegistrator.as_ptr());
    ref_instance->LoadFromRawData(base_type, raw_data);

    return ref_instance;
}

auto ConvertRefTypeScriptObjectToRawData(const BaseTypeDesc& base_type, nptr<void> as_obj) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(base_type.IsRefType, "Base type is not a reference type");
    FO_VERIFY_AND_THROW(base_type.RefType, "Reference type descriptor is null");
    FO_VERIFY_AND_THROW(base_type.RefType->FieldsRegistrator, "Reference type has no fields registrator");

    if (!as_obj) {
        return {};
    }

    auto ref_instance = as_obj.reinterpret_as<DynamicRefTypeInstance>();
    const_span<uint8_t> raw_data = ref_instance->GetSerializedRawData(base_type);

    return {raw_data.begin(), raw_data.end()};
}

auto CreateRefTypeScriptObjectFromProperty(ptr<const Property> prop, span<const uint8_t> raw_data) -> refcount_ptr<DynamicRefTypeInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsBaseTypeRefType(), "Property base type is not a reference type");

    return CreateRefTypeScriptObjectFromRawData(prop->GetBaseType(), raw_data);
}

auto ConvertRefTypeScriptObjectToProperty(ptr<const Property> prop, nptr<void> as_obj) -> PropertyRawData
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(prop->IsBaseTypeRefType(), "Property base type is not a reference type");
    const auto raw_data = ConvertRefTypeScriptObjectToRawData(prop->GetBaseType(), as_obj);

    PropertyRawData prop_data;

    if (!raw_data.empty()) {
        prop_data.Set(raw_data.data(), raw_data.size());
    }

    return prop_data;
}

auto GetGenericObject(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> object = gen->GetObject();
    FO_STRONG_ASSERT(object, "Generic call object is null");
    return object;
}

auto GetGenericAuxiliary(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> auxiliary = gen->GetAuxiliary();
    FO_STRONG_ASSERT(auxiliary, "Generic call auxiliary is null");
    return auxiliary;
}

auto GetGenericArgAddress(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    return gen->GetArgAddress(arg_index);
}

auto GetGenericAddressArg(ptr<AngelScript::asIScriptGeneric> gen, uint32_t arg_index) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> arg_address = gen->GetAddressOfArg(arg_index);
    FO_STRONG_ASSERT(arg_address, "Generic call argument address is null");
    return arg_address;
}

void ReturnGenericEntity(ptr<AngelScript::asIScriptGeneric> gen, nptr<Entity> entity) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) Entity*(entity.get());
}

void ReturnGenericScriptArray(ptr<AngelScript::asIScriptGeneric> gen, ptr<ScriptArray> arr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr.get());
}

void ReturnGenericScriptArray(ptr<AngelScript::asIScriptGeneric> gen, refcount_ptr<ScriptArray>&& arr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr.release_ownership());
}

void SetScriptObjectFromHandleSlot(ptr<AngelScript::asIScriptContext> ctx, ptr<void> slot)
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    FO_AS_VERIFY(ctx->SetObject(NativeDataProvider::ReadHandleSlot(slot).get()));
}

void SetScriptArgObjectFromHandleSlot(ptr<AngelScript::asIScriptContext> ctx, uint32_t arg_index, ptr<void> slot)
{
    FO_NO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    FO_AS_VERIFY(ctx->SetArgObject(arg_index, NativeDataProvider::ReadHandleSlot(slot).get()));
}

auto GetNullableHandleSlotAddress(ptr<nptr<void>> slot) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto slot_address = make_ptr(slot->get_pp()).reinterpret_as<void>();
    return slot_address;
}

auto GetContextAddressOfArg(ptr<AngelScript::asIScriptContext> ctx, uint32_t arg_index) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> arg_address = ctx->GetAddressOfArg(arg_index);
    FO_STRONG_ASSERT(arg_address, "Context argument address is null");
    return arg_address;
}

auto GetContextAddressOfReturnValue(ptr<AngelScript::asIScriptContext> ctx) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> return_value = ctx->GetAddressOfReturnValue();
    FO_STRONG_ASSERT(return_value, "Context return value address is null");
    return return_value;
}

auto MakeAngelScriptFuncDescBorrow(ptr<ScriptFuncDesc> func_desc, refcount_ptr<AngelScript::asIScriptFunction> func_lifetime) -> unique_del_ptr<ScriptFuncDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    return make_unique_del_ptr(func_desc, [func_lifetime = std::move(func_lifetime)](ptr<ScriptFuncDesc> released_func_desc) noexcept { ignore_unused(released_func_desc, func_lifetime); });
}

FO_END_NAMESPACE

#endif
