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

FO_BEGIN_NAMESPACE

[[noreturn]] void ThrowScriptCoreException(string_view file, int32 line, int32 result)
{
    FO_NO_STACK_TRACE_ENTRY();

    const string file_name = strex(file).extract_file_name().erase_file_extension();
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

    return type.Name;
}

auto MakeScriptTypeName(const ComplexTypeDesc& type) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(type);

    string result;

    if (type.Kind == ComplexTypeKind::DictOfArray) {
        FO_RUNTIME_ASSERT(type.KeyType);
        result = strex("dict<{},{}[]>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Dict) {
        FO_RUNTIME_ASSERT(type.KeyType);
        result = strex("dict<{},{}>", MakeScriptTypeName(type.KeyType.value()), MakeScriptTypeName(type.BaseType));
    }
    else if (type.Kind == ComplexTypeKind::Array) {
        result = strex("{}[]", MakeScriptTypeName(type.BaseType));
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

auto MakeScriptArgName(const ComplexTypeDesc& type) -> string
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
    }
    else {
        result += "@+";
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

        result += MakeScriptArgName(arg.Type);
        result += " ";
        result += arg.Name;
    }

    return result;
}

auto MakeScriptReturnName(const ComplexTypeDesc& type, bool pass_ownership) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!type) {
        return "void";
    }

    string result = MakeScriptArgName(type);

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
            result = strex("dict<{},{}[]>", MakeScriptTypeName(prop->GetDictKeyType()), MakeScriptTypeName(prop->GetBaseType()));
        }
        else {
            result = strex("dict<{},{}>", MakeScriptTypeName(prop->GetDictKeyType()), MakeScriptTypeName(prop->GetBaseType()));
        }
    }
    else if (prop->IsArray()) {
        result = strex("{}[]", MakeScriptTypeName(prop->GetBaseType()));
    }
    else {
        result = MakeScriptTypeName(prop->GetBaseType());
    }

    return result;
}

auto CreateScriptArray(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptArray*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    FO_RUNTIME_ASSERT(type_id);
    auto* as_type_info = as_engine->GetTypeInfoById(type_id);
    FO_RUNTIME_ASSERT(as_type_info);
    auto* as_array = ScriptArray::Create(as_type_info);
    FO_RUNTIME_ASSERT(as_array);
    return as_array;
}

auto CreateScriptDict(AngelScript::asIScriptEngine* as_engine, const char* type) -> ScriptDict*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    FO_RUNTIME_ASSERT(type_id);
    auto* as_type_info = as_engine->GetTypeInfoById(type_id);
    FO_RUNTIME_ASSERT(as_type_info);
    auto* as_dict = ScriptDict::Create(as_type_info);
    FO_RUNTIME_ASSERT(as_dict);
    return as_dict;
}

auto CalcConstructAddrSpace(const Property* prop) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            return sizeof(hstring);
        }
        else if (prop->IsBaseTypeEnum()) {
            return sizeof(int32);
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
        const auto* hash_resolver = prop->GetRegistrator()->GetHashResolver();
        return hash_resolver->ResolveHash(hash);
    };
    const auto resolve_enum = [](const void* eptr, size_t elen) -> int32 {
        int32 result = 0;
        MemCopy(&result, eptr, elen);
        return result;
    };

    const auto* data = prop_data.GetPtrAs<uint8>();
    const auto data_size = prop_data.GetSize();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            FO_RUNTIME_ASSERT(data_size == sizeof(hstring::hash_t));
            new (construct_addr) hstring(resolve_hash(data));
        }
        else if (prop->IsBaseTypeEnum()) {
            FO_RUNTIME_ASSERT(data_size != 0);
            FO_RUNTIME_ASSERT(data_size <= sizeof(int32));
            MemFill(construct_addr, 0, sizeof(int32));
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
    else if (prop->IsString()) {
        new (construct_addr) string(reinterpret_cast<const char*>(data), data_size);
    }
    else if (prop->IsArray()) {
        auto* arr = CreateScriptArray(as_engine, MakeScriptPropertyName(prop).c_str());

        if (prop->IsArrayOfString()) {
            if (data_size != 0) {
                uint32 arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);

                arr->Resize(numeric_cast<int32>(arr_size));

                for (uint32 i = 0; i < arr_size; i++) {
                    uint32 str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr->SetValue(numeric_cast<int32>(i), cast_to_void(&str));

                    data += str_size;
                }
            }
        }
        else if (prop->IsBaseTypeHash()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32>(arr_size));

                for (uint32 i = 0; i < arr_size; i++) {
                    const auto hvalue = resolve_hash(data);
                    arr->SetValue(numeric_cast<int32>(i), cast_to_void(&hvalue));

                    data += sizeof(hstring::hash_t);
                }
            }
        }
        else if (prop->IsBaseTypeEnum()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32>(arr_size));

                if (prop->GetBaseSize() == sizeof(int32)) {
                    MemCopy(arr->At(0), data, data_size);
                }
                else {
                    auto* dest = cast_from_void<int32*>(arr->At(0));

                    for (uint32 i = 0; i < arr_size; i++) {
                        *(dest + i) = 0;
                        MemCopy(dest + i, data + i * prop->GetBaseSize(), prop->GetBaseSize());
                    }
                }
            }
        }
        else if (prop->IsBaseTypePrimitive()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32>(arr_size));
                MemCopy(arr->At(0), data, data_size);
            }
        }
        else if (prop->IsBaseTypeStruct()) {
            if (data_size != 0) {
                const auto arr_size = numeric_cast<uint32>(data_size / prop->GetBaseSize());
                arr->Resize(numeric_cast<int32>(arr_size));

                for (uint32 i = 0; i < arr_size; i++) {
                    MemCopy(arr->At(numeric_cast<int32>(i)), data, prop->GetBaseSize());
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
        ScriptDict* dict = CreateScriptDict(as_engine, MakeScriptPropertyName(prop).c_str());

        if (data_size != 0) {
            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    uint32 arr_size;
                    MemCopy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(arr_size);

                    auto* arr = CreateScriptArray(as_engine, strex("{}[]", prop->GetBaseTypeName()).c_str());

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            arr->Resize(numeric_cast<int32>(arr_size));

                            for (uint32 i = 0; i < arr_size; i++) {
                                uint32 str_size;
                                MemCopy(&str_size, data, sizeof(str_size));
                                data += sizeof(str_size);

                                string str(reinterpret_cast<const char*>(data), str_size);
                                arr->SetValue(numeric_cast<int32>(i), cast_to_void(&str));
                                data += str_size;
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            arr->Resize(numeric_cast<int32>(arr_size));

                            for (uint32 i = 0; i < arr_size; i++) {
                                const auto hvalue = resolve_hash(data);
                                arr->SetValue(numeric_cast<int32>(i), cast_to_void(&hvalue));

                                data += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            arr->Resize(numeric_cast<int32>(arr_size));

                            if (prop->GetBaseSize() == 4) {
                                MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            }
                            else {
                                auto* dest = cast_from_void<int32*>(arr->At(0));
                                for (uint32 i = 0; i < arr_size; i++) {
                                    MemCopy(dest + i, data + i * prop->GetBaseSize(), prop->GetBaseSize());
                                }
                            }

                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            arr->Resize(numeric_cast<int32>(arr_size));

                            MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            arr->Resize(numeric_cast<int32>(arr_size));

                            for (uint32 i = 0; i < arr_size; i++) {
                                MemCopy(arr->At(numeric_cast<int32>(i)), data, prop->GetBaseSize());
                                data += prop->GetBaseSize();
                            }
                        }
                        else {
                            FO_UNREACHABLE_PLACE();
                        }
                    }

                    if (prop->IsDictKeyString()) {
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
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
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    uint32 str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint32);

                    auto str = string {reinterpret_cast<const char*>(data), str_size};

                    if (prop->IsDictKeyString()) {
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
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
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeyTypeSize();
                    }

                    if (prop->IsDictKeyString()) {
                        const uint32 key_len = *reinterpret_cast<const uint32*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&key_str), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&key_str), cast_to_void(data));
                        }
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&hkey), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&hkey), cast_to_void(data));
                        }
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeyTypeSize());

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(&ekey), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(&ekey), cast_to_void(data));
                        }
                    }
                    else {
                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(cast_to_void(key), cast_to_void(&hvalue));
                        }
                        else {
                            dict->Set(cast_to_void(key), cast_to_void(data));
                        }
                    }

                    data += prop->GetBaseSize();
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

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            const auto hash = cast_from_void<const hstring*>(as_obj)->as_hash();
            FO_RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hash));
            prop_data.SetAs<hstring::hash_t>(hash);
        }
        else {
            prop_data.Set(as_obj, prop->GetBaseSize());
        }
    }
    else if (prop->IsString()) {
        const auto& str = *cast_from_void<const string*>(as_obj);
        prop_data.Pass(str.data(), str.length());
    }
    else if (prop->IsArray()) {
        const auto* arr = *cast_from_void<ScriptArray**>(as_obj);

        if (prop->IsArrayOfString()) {
            const auto arr_size = numeric_cast<uint32>(arr != nullptr ? arr->GetSize() : 0);

            if (arr_size != 0) {
                // Calculate size
                size_t data_size = sizeof(uint32);

                for (uint32 i = 0; i < arr_size; i++) {
                    const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32>(i)));
                    data_size += sizeof(uint32) + numeric_cast<uint32>(str.length());
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                MemCopy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint32);

                for (uint32 i = 0; i < arr_size; i++) {
                    const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32>(i)));

                    const auto str_size = numeric_cast<uint32>(str.length());
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
            const uint32 arr_size = (arr != nullptr ? arr->GetSize() : 0);
            const size_t data_size = (arr != nullptr ? arr_size * prop->GetBaseSize() : 0);

            if (data_size != 0) {
                if (prop->IsBaseTypeHash()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint32 i = 0; i < arr_size; i++) {
                        const auto hash = cast_from_void<const hstring*>(arr->At(numeric_cast<int32>(i)))->as_hash();
                        MemCopy(buf, &hash, sizeof(hstring::hash_t));
                        buf += prop->GetBaseSize();
                    }
                }
                else if (prop->IsBaseTypeEnum()) {
                    if (prop->GetBaseSize() == sizeof(int32)) {
                        prop_data.Pass(arr->At(0), data_size);
                    }
                    else {
                        auto* buf = prop_data.Alloc(data_size);

                        for (uint32 i = 0; i < arr_size; i++) {
                            const auto e = *cast_from_void<const int32*>(arr->At(numeric_cast<int32>(i)));
                            MemCopy(buf, &e, prop->GetBaseSize());
                            buf += prop->GetBaseSize();
                        }
                    }
                }
                else if (prop->IsBaseTypePrimitive()) {
                    prop_data.Pass(arr->At(0), data_size);
                }
                else if (prop->IsBaseTypeStruct()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint32 i = 0; i < arr_size; i++) {
                        MemCopy(buf, arr->At(numeric_cast<int32>(i)), prop->GetBaseSize());
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
                        const auto key_len = numeric_cast<uint32>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    const auto* arr = *cast_from_void<const ScriptArray**>(value);
                    const uint32 arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    data_size += sizeof(arr_size);

                    if (prop->IsDictOfArrayOfString()) {
                        for (uint32 i = 0; i < arr_size; i++) {
                            const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32>(i)));
                            data_size += sizeof(uint32) + numeric_cast<uint32>(str.length());
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
                        const auto key_len = numeric_cast<uint32>(key_str.length());
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
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *cast_from_void<const int32*>(key);
                        MemCopy(buf, &ekey, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }

                    const uint32 arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    MemCopy(buf, &arr_size, sizeof(uint32));
                    buf += sizeof(arr_size);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint32 i = 0; i < arr_size; i++) {
                                const auto& str = *cast_from_void<const string*>(arr->At(numeric_cast<int32>(i)));
                                const auto str_size = numeric_cast<uint32>(str.length());

                                MemCopy(buf, &str_size, sizeof(uint32));
                                buf += sizeof(str_size);

                                if (str_size != 0) {
                                    MemCopy(buf, str.c_str(), str_size);
                                    buf += str_size;
                                }
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            for (uint32 i = 0; i < arr_size; i++) {
                                const auto hash = cast_from_void<const hstring*>(arr->At(numeric_cast<int32>(i)))->as_hash();
                                MemCopy(buf, &hash, sizeof(hstring::hash_t));
                                buf += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            if (prop->GetBaseSize() == sizeof(int32)) {
                                MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                                buf += arr_size * prop->GetBaseSize();
                            }
                            else {
                                for (uint32 i = 0; i < arr_size; i++) {
                                    const auto e = *cast_from_void<const int32*>(arr->At(numeric_cast<int32>(i)));
                                    MemCopy(buf, &e, prop->GetBaseSize());
                                    buf += prop->GetBaseSize();
                                }
                            }
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                            buf += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            for (uint32 i = 0; i < arr_size; i++) {
                                MemCopy(buf, arr->At(numeric_cast<int32>(i)), prop->GetBaseSize());
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
                        const auto key_len = numeric_cast<uint32>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeyTypeSize();
                    }

                    const auto& str = *cast_from_void<const string*>(value);
                    const auto str_size = numeric_cast<uint32>(str.length());

                    data_size += sizeof(str_size) + str_size;
                }

                // Make buffer
                uint8* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto& str = *cast_from_void<const string*>(value);

                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *cast_from_void<const string*>(key);
                        const auto key_len = numeric_cast<uint32>(key_str.length());
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
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *cast_from_void<const int32*>(key);
                        MemCopy(buf, &ekey, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeyTypeSize());
                        buf += prop->GetDictKeyTypeSize();
                    }

                    const auto str_size = numeric_cast<uint32>(str.length());
                    MemCopy(buf, &str_size, sizeof(uint32));
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

                const auto value_element_size = prop->GetBaseSize();

                for (const auto& key : dict->GetMap() | std::views::keys) {
                    const auto& key_str = *cast_from_void<const string*>(key);
                    const auto key_len = numeric_cast<uint32>(key_str.length());
                    data_size += sizeof(key_len) + key_len;
                    data_size += value_element_size;
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    const auto& key_str = *cast_from_void<const string*>(key);
                    const auto key_len = numeric_cast<uint32>(key_str.length());

                    MemCopy(buf, &key_len, sizeof(key_len));
                    buf += sizeof(key_len);
                    MemCopy(buf, key_str.c_str(), key_len);
                    buf += key_len;

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = cast_from_void<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, value_element_size);
                    }
                    else {
                        MemCopy(buf, value, value_element_size);
                    }

                    buf += value_element_size;
                }
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeyTypeSize();
            const auto value_element_size = prop->GetBaseSize();
            const auto whole_element_size = key_element_size + value_element_size;
            const auto data_size = dict != nullptr ? dict->GetSize() * whole_element_size : 0;

            if (data_size != 0) {
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict->GetMap()) {
                    if (prop->IsDictKeyHash()) {
                        const auto hkey = cast_from_void<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, key_element_size);
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *cast_from_void<const int32*>(key);
                        MemCopy(buf, &ekey, key_element_size);
                    }
                    else {
                        MemCopy(buf, key, key_element_size);
                    }

                    buf += key_element_size;

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = cast_from_void<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, value_element_size);
                    }
                    else {
                        MemCopy(buf, value, value_element_size);
                    }

                    buf += value_element_size;
                }
            }
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    return prop_data;
}

auto GetScriptObjectInfo(const void* ptr, int32 type_id) -> string
{
    FO_STACK_TRACE_ENTRY();

    switch (type_id) {
    case AngelScript::asTYPEID_VOID:
        return "void";
    case AngelScript::asTYPEID_BOOL:
        return strex("bool: {}", *cast_from_void<const bool*>(ptr) ? "true" : "false");
    case AngelScript::asTYPEID_INT8:
        return strex("int8: {}", *cast_from_void<const int8*>(ptr));
    case AngelScript::asTYPEID_INT16:
        return strex("int16: {}", *cast_from_void<const int16*>(ptr));
    case AngelScript::asTYPEID_INT32:
        return strex("int32: {}", *cast_from_void<const int32*>(ptr));
    case AngelScript::asTYPEID_INT64:
        return strex("int64: {}", *cast_from_void<const int64*>(ptr));
    case AngelScript::asTYPEID_UINT8:
        return strex("uint8: {}", *cast_from_void<const uint8*>(ptr));
    case AngelScript::asTYPEID_UINT16:
        return strex("uint16: {}", *cast_from_void<const uint16*>(ptr));
    case AngelScript::asTYPEID_UINT32:
        return strex("uint32: {}", *cast_from_void<const uint32*>(ptr));
    case AngelScript::asTYPEID_UINT64:
        return strex("uint64: {}", *cast_from_void<const uint64*>(ptr));
    case AngelScript::asTYPEID_FLOAT:
        return strex("float32: {}", *cast_from_void<const float32*>(ptr));
    case AngelScript::asTYPEID_DOUBLE:
        return strex("float64: {}", *cast_from_void<const float64*>(ptr));
    default:
        break;
    }

    // Todo: GetScriptObjectInfo add detailed info about object
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
        const auto enum_value = *cast_from_void<const int32*>(ptr);

        if (meta->IsValidBaseType(type_name)) {
            bool failed = false;
            const string& enum_value_name = meta->ResolveEnumValueName(type_name, enum_value, &failed);

            if (!failed) {
                return strex("{}: {}", type_name, enum_value_name);
            }
        }

        for (AngelScript::asUINT i = 0; i < enum_value_count; i++) {
            int32 check_enum_value = 0;
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

FO_END_NAMESPACE

#endif
