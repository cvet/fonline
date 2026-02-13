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

#include "AngelScriptCall.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"

FO_BEGIN_NAMESPACE

auto ScriptDataAccessor::GetArraySize(void* data) const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* arr = *cast_from_void<ScriptArray**>(data);
    return arr != nullptr ? numeric_cast<size_t>(arr->GetSize()) : 0;
}

auto ScriptDataAccessor::GetArrayElement(void* data, size_t index) const -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* arr = *cast_from_void<ScriptArray**>(data);
    FO_RUNTIME_ASSERT(arr);
    return arr->At(numeric_cast<int32>(index));
}

auto ScriptDataAccessor::GetDictSize(void* data) const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* dict = *cast_from_void<ScriptDict**>(data);
    return dict != nullptr ? numeric_cast<size_t>(dict->GetSize()) : 0;
}

auto ScriptDataAccessor::GetDictElement(void* data, size_t index) const -> pair<void*, void*>
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* dict = *cast_from_void<ScriptDict**>(data);
    FO_RUNTIME_ASSERT(dict);
    const auto it = std::next(dict->GetMap().begin(), static_cast<ptrdiff_t>(index));
    return pair(it->first, it->second);
}

auto ScriptDataAccessor::GetCallback(void* data) const -> unique_del_ptr<ScriptFuncDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(data);

    if (func != nullptr) {
        auto* func_desc = IndexScriptFunc(func);
        FO_RUNTIME_ASSERT(func_desc->Call);
        return unique_del_ptr<ScriptFuncDesc>(func_desc, [func_ = refcount_ptr(func)](auto&&) { });
    }

    return nullptr;
}

void ScriptDataAccessor::ClearArray(void* data) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* arr = *cast_from_void<ScriptArray**>(data);
    FO_RUNTIME_ASSERT(arr);
    arr->Resize(0);
}

void ScriptDataAccessor::AddArrayElement(void* data, void* value) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* arr = *cast_from_void<ScriptArray**>(data);
    FO_RUNTIME_ASSERT(arr);
    arr->InsertLast(value);
}

void ScriptDataAccessor::ClearDict(void* data) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* dict = *cast_from_void<ScriptDict**>(data);
    FO_RUNTIME_ASSERT(dict);
    dict->Clear();
}

void ScriptDataAccessor::AddDictElement(void* data, void* key, void* value) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* dict = *cast_from_void<ScriptDict**>(data);
    FO_RUNTIME_ASSERT(dict);
    dict->Set(key, value);
}

auto IndexScriptFunc(AngelScript::asIScriptFunction* func) -> ScriptFuncDesc*
{
    FO_STACK_TRACE_ENTRY();

    if (auto* func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData()); func_desc != nullptr) {
        return func_desc;
    }

    auto* as_engine = func->GetEngine();
    const auto* meta = GetEngineMetadata(as_engine);

    const auto resolve_type = [&](int32 type_id, AngelScript::asDWORD flags, bool is_ret) -> ComplexTypeDesc {
        FO_RUNTIME_ASSERT(type_id != AngelScript::asTYPEID_VOID);

        const auto get_type_name = [&](int32 tid) -> string_view {
            switch (tid) {
            case AngelScript::asTYPEID_VOID:
                return "void";
            case AngelScript::asTYPEID_BOOL:
                return "bool";
            case AngelScript::asTYPEID_INT8:
                return "int8";
            case AngelScript::asTYPEID_INT16:
                return "int16";
            case AngelScript::asTYPEID_INT32:
                return "int32";
            case AngelScript::asTYPEID_INT64:
                return "int64";
            case AngelScript::asTYPEID_UINT8:
                return "uint8";
            case AngelScript::asTYPEID_UINT16:
                return "uint16";
            case AngelScript::asTYPEID_UINT32:
                return "uint32";
            case AngelScript::asTYPEID_UINT64:
                return "uint64";
            case AngelScript::asTYPEID_FLOAT:
                return "float32";
            case AngelScript::asTYPEID_DOUBLE:
                return "float64";
            default:
                break;
            }

            const auto* ti = as_engine->GetTypeInfoById(tid);
            FO_RUNTIME_ASSERT(ti);
            return ti->GetName();
        };

        const auto* as_type_info = as_engine->GetTypeInfoById(type_id);

        ComplexTypeDesc type;

        if (as_type_info != nullptr && string_view(as_type_info->GetName()) == "dict") {
            const auto key_name = get_type_name(as_type_info->GetSubTypeId(0));
            const auto value_name = get_type_name(as_type_info->GetSubTypeId(1));

            if (!meta->IsValidBaseType(key_name)) {
                return {};
            }

            if (string_view(value_name) == "array") {
                const auto value_name2 = get_type_name(as_type_info->GetSubType(1)->GetSubTypeId());

                if (!meta->IsValidBaseType(value_name2)) {
                    return {};
                }

                type.Kind = ComplexTypeKind::DictOfArray;
                type.KeyType = meta->GetBaseType(key_name);
                type.BaseType = meta->GetBaseType(value_name2);
            }
            else {
                if (!meta->IsValidBaseType(value_name)) {
                    return {};
                }

                type.Kind = ComplexTypeKind::Dict;
                type.KeyType = meta->GetBaseType(key_name);
                type.BaseType = meta->GetBaseType(value_name);
            }
        }
        else if (as_type_info != nullptr && string_view(as_type_info->GetName()) == "array") {
            const auto name = get_type_name(as_type_info->GetSubTypeId());

            if (!meta->IsValidBaseType(name)) {
                return {};
            }

            type.Kind = ComplexTypeKind::Array;
            type.BaseType = meta->GetBaseType(name);
        }
        else {
            const auto name = get_type_name(type_id);

            if (!meta->IsValidBaseType(name)) {
                return {};
            }

            type.Kind = ComplexTypeKind::Simple;
            type.BaseType = meta->GetBaseType(name);
        }

        if ((flags & AngelScript::asTM_INOUTREF) != 0) {
            if (is_ret) {
                return {};
            }

            type.IsMutable = true;
        }

        FO_RUNTIME_ASSERT(type);
        return type;
    };

    const auto func_name = GetScriptFuncName(func, meta->Hashes);
    auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();

    func_desc->Name = func_name;
    func_desc->DelegateObj = std::bit_cast<uintptr_t>(func->GetDelegateObject());

    for (AngelScript::asUINT p = 0; p < func->GetParamCount(); p++) {
        int32 as_result = 0;
        int32 param_type_id;
        AngelScript::asDWORD param_flags;
        const char* param_name;
        FO_AS_VERIFY(func->GetParam(p, &param_type_id, &param_flags, &param_name));

        auto arg_type = resolve_type(param_type_id, param_flags, false);
        func_desc->Args.emplace_back(ArgDesc {.Name = param_name != nullptr ? param_name : "", .Type = std::move(arg_type)});
    }

    AngelScript::asDWORD ret_flags = 0;
    const int32 ret_type_id = func->GetReturnTypeId(&ret_flags);
    const bool is_void_ret = ret_type_id == AngelScript::asTYPEID_VOID;

    if (!is_void_ret) {
        func_desc->Ret = resolve_type(ret_type_id, ret_flags, true);
    }

    const bool call_supported = (is_void_ret || !!func_desc->Ret) && !std::ranges::any_of(func_desc->Args, [](auto&& a) { return !a.Type; });

    if (call_supported) {
        func_desc->Call = [func](FuncCallData& call) FO_DEFERRED { ScriptFuncCall(func, call); };
    }

    func->SetUserData(cast_to_void(func_desc.release()));
    return cast_from_void<ScriptFuncDesc*>(func->GetUserData());
}

void ScriptGenericCall(AngelScript::asIScriptGeneric* gen, bool add_obj, const function<void(FuncCallData&)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    FuncCallData call {.Accessor = &SCRIPT_DATA_ACCESSOR};
    const size_t args_count = gen->GetArgCount() + (add_obj ? 1 : 0);
    array<void*, MAX_CALL_ARGS> args_data;
    call.ArgsData = span(args_data).subspan(0, args_count);
    void* this_obj = gen->GetObject();

    if (add_obj) {
        FO_RUNTIME_ASSERT(this_obj);

        for (AngelScript::asUINT index = 0; index < args_count; index++) {
            args_data[index] = index == 0 ? static_cast<void*>(&this_obj) : gen->GetAddressOfArg(index - 1);
        }
    }
    else {
        for (AngelScript::asUINT index = 0; index < args_count; index++) {
            args_data[index] = gen->GetAddressOfArg(index);
        }
    }

    const auto ret_type_id = gen->GetReturnTypeId();

    if (ret_type_id != AngelScript::asTYPEID_VOID) {
        call.RetData = gen->GetAddressOfReturnLocation();
        auto* as_engine = gen->GetEngine();

        if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            const auto* as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
            FO_RUNTIME_ASSERT(as_ret_type);
            const bool is_ref_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0;
            const bool is_value_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_VALUE) != 0;
            const bool is_array = is_ref_type && as_ret_type->GetName() == string_view("array");
            const bool is_dict = is_ref_type && as_ret_type->GetName() == string_view("dict");

            if (is_ref_type && !is_array && !is_dict) {
                *static_cast<void**>(call.RetData) = nullptr;
            }
            else if (is_ref_type) {
                void* ret_obj = as_engine->CreateScriptObject(as_ret_type);
                FO_RUNTIME_ASSERT(ret_obj);
                *static_cast<void**>(call.RetData) = ret_obj;
            }
            else if (is_value_type) {
                void* ret_obj = as_engine->CreateScriptObject(as_ret_type);
                FO_RUNTIME_ASSERT(ret_obj);
                FO_AS_VERIFY(gen->SetReturnObject(ret_obj));
                as_engine->ReleaseScriptObject(ret_obj, as_ret_type);
            }
            else {
                FO_UNREACHABLE_PLACE();
            }
        }
        else {
            MemFill(call.RetData, 0, as_engine->GetSizeOfPrimitiveType(ret_type_id));
        }
    }

    try {
        callback(call);
    }
    catch (...) {
        if (ret_type_id != AngelScript::asTYPEID_VOID && (ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            FO_RUNTIME_ASSERT(call.RetData);
            void* ret_obj = *static_cast<void**>(call.RetData);

            if (ret_obj != nullptr) {
                auto* as_engine = gen->GetEngine();
                const auto* as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
                as_engine->ReleaseScriptObject(ret_obj, as_ret_type);
            }
        }

        throw;
    }
}

void ScriptFuncCall(AngelScript::asIScriptFunction* func, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(func);
    FO_RUNTIME_ASSERT(call.ArgsData.size() == func->GetParamCount());
    FO_RUNTIME_ASSERT((call.RetData != nullptr) == (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID));

    int32 as_result = 0;
    auto* as_engine = func->GetEngine();
    auto* backend = GetScriptBackend(as_engine);
    auto* context_mngr = backend->GetContextMngr();
    auto* ctx = context_mngr->PrepareContext(func);
    auto return_ctx = scope_exit([&]() noexcept { context_mngr->ReturnContext(ctx); });
    const auto* func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData());
    FO_RUNTIME_ASSERT(func_desc);
    FO_RUNTIME_ASSERT(func_desc->Call);

    if (call.Accessor && call.Accessor->GetBackendIndex() == ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX) {
        // Direct call AS to AS
        for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
            const auto& arg_type = func_desc->Args[i].Type;
            const auto& base_type = arg_type.BaseType;
            void* arg_data = call.ArgsData[i];

            if (arg_type.Kind == ComplexTypeKind::Simple) {
                if (arg_type.IsMutable) {
                    FO_AS_VERIFY(ctx->SetArgAddress(i, *static_cast<void**>(arg_data)));
                }
                else if (base_type.IsEntity || base_type.IsRefType) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, *static_cast<void**>(arg_data)));
                }
                else if (base_type.IsObject) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, arg_data));
                }
                else if (base_type.IsEnum || base_type.IsPrimitive) {
                    MemCopy(ctx->GetAddressOfArg(i), arg_data, base_type.Size);
                }
                else {
                    throw NotSupportedException("Invalid script func call - invalid simple type", base_type.Name);
                }
            }
            else {
                FO_AS_VERIFY(ctx->SetArgObject(i, *static_cast<void**>(arg_data)));
            }
        }

        if (context_mngr->RunContext(ctx, !func_desc->Ret)) {
            if (func_desc->Ret) {
                FO_RUNTIME_ASSERT(call.RetData);
                const auto ret_type_id = func->GetReturnTypeId();

                if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
                    const auto* as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
                    FO_RUNTIME_ASSERT(as_ret_type);
                    void* ret_obj = ctx->GetReturnObject();

                    if ((ret_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
                        *static_cast<void**>(call.RetData) = ret_obj;
                        as_engine->AddRefScriptObject(ret_obj, as_ret_type);
                    }
                    else {
                        if (ret_obj != nullptr) {
                            FO_AS_VERIFY(as_engine->AssignScriptObject(*static_cast<void**>(call.RetData), ret_obj, as_ret_type));
                        }
                        else {
                            *static_cast<void**>(call.RetData) = as_engine->CreateScriptObject(as_ret_type);
                        }
                    }
                }
                else {
                    MemCopy(call.RetData, ctx->GetAddressOfReturnValue(), func_desc->Ret.BaseType.Size);
                }
            }
        }
    }
    else {
        array<void*, MAX_CALL_ARGS> mutable_data = {};
        array<int32, MAX_CALL_ARGS> enum_mutable_data = {};

        for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
            const auto& arg_type = func_desc->Args[i].Type;
            const auto& base_type = arg_type.BaseType;
            auto* arg_data = call.ArgsData[i];

            if (arg_type.Kind == ComplexTypeKind::Simple) {
                if (arg_type.IsMutable) {
                    if (base_type.IsEnum && base_type.EnumUnderlyingType->Size != sizeof(int32)) {
                        mutable_data[i] = cast_to_void(&enum_mutable_data[i]);
                        MemCopy(mutable_data[i], arg_data, base_type.Size);
                        FO_AS_VERIFY(ctx->SetArgAddress(i, mutable_data[i]));
                    }
                    else {
                        FO_AS_VERIFY(ctx->SetArgAddress(i, arg_data));
                    }
                }
                else if (base_type.IsEntity || base_type.IsRefType) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, *static_cast<void**>(arg_data)));
                }
                else if (base_type.IsObject) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, static_cast<void*>(arg_data)));
                }
                else if (base_type.IsEnum) {
                    int32 enum_value = 0;
                    MemCopy(&enum_value, arg_data, base_type.Size);
                    FO_AS_VERIFY(ctx->SetArgDWord(i, enum_value));
                }
                else if (base_type.IsPrimitive) {
                    MemCopy(ctx->GetAddressOfArg(i), arg_data, base_type.Size);
                }
                else {
                    throw NotSupportedException("Invalid script func call - invalid simple type", base_type.Name);
                }
            }
            else if (arg_type.Kind == ComplexTypeKind::Array) {
                const size_t arr_size = call.Accessor->GetArraySize(arg_data);
                auto* arr = CreateScriptArray(as_engine, MakeScriptTypeName(arg_type).c_str());
                arr->Reserve(numeric_cast<int32>(arr_size));

                for (size_t j = 0; j < arr_size; j++) {
                    void* elem = call.Accessor->GetArrayElement(arg_data, j);
                    arr->InsertLast(elem);
                }

                FO_AS_VERIFY(ctx->SetArgObject(i, arr));

                if (arg_type.IsMutable) {
                    mutable_data[i] = arr;
                }
                else {
                    arr->Release();
                }
            }
            else if (arg_type.Kind == ComplexTypeKind::Dict) {
                const size_t dict_size = call.Accessor->GetDictSize(arg_data);
                auto* dict = CreateScriptDict(as_engine, MakeScriptTypeName(arg_type).c_str());

                for (size_t j = 0; j < dict_size; j++) {
                    const auto elem = call.Accessor->GetDictElement(arg_data, j);
                    dict->Set(elem.first, elem.second);
                }

                FO_AS_VERIFY(ctx->SetArgObject(i, dict));

                if (arg_type.IsMutable) {
                    mutable_data[i] = dict;
                }
                else {
                    dict->Release();
                }
            }
            else {
                throw NotSupportedException(FO_LINE_STR);
            }
        }

        if (context_mngr->RunContext(ctx, !func_desc->Ret)) {
            if (func_desc->Ret) {
                FO_RUNTIME_ASSERT(call.RetData);
                const auto ret_type_id = func->GetReturnTypeId();

                if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
                    const auto* as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
                    FO_RUNTIME_ASSERT(as_ret_type);
                    void*& cur_obj = *static_cast<void**>(call.RetData);
                    void* ret_obj = ctx->GetReturnObject();

                    if (cur_obj != nullptr && ret_obj != nullptr) {
                        FO_AS_VERIFY(as_engine->AssignScriptObject(cur_obj, ret_obj, as_ret_type));
                    }
                    else {
                        if (cur_obj != nullptr) {
                            as_engine->ReleaseScriptObject(cur_obj, as_ret_type);
                            cur_obj = nullptr;
                        }
                        if (ret_obj != nullptr) {
                            cur_obj = as_engine->CreateScriptObjectCopy(ret_obj, as_ret_type);
                            FO_RUNTIME_ASSERT(cur_obj);
                        }
                    }
                }
                else {
                    FO_RUNTIME_ASSERT(as_engine->GetSizeOfPrimitiveType(ret_type_id) == numeric_cast<int32>(func_desc->Ret.BaseType.Size));
                    MemCopy(call.RetData, ctx->GetAddressOfReturnValue(), func_desc->Ret.BaseType.Size);
                }
            }

            for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
                if (mutable_data[i] != nullptr) {
                    const auto& arg_type = func_desc->Args[i].Type;
                    auto* arg_data = call.ArgsData[i];
                    FO_RUNTIME_ASSERT(arg_type.IsMutable);

                    if (arg_type.Kind == ComplexTypeKind::Simple) {
                        FO_RUNTIME_ASSERT(arg_type.BaseType.IsEnum);
                        MemCopy(arg_data, mutable_data[i], arg_type.BaseType.Size);
                    }
                    else if (arg_type.Kind == ComplexTypeKind::Array) {
                        const auto* arr = cast_from_void<ScriptArray*>(mutable_data[i]);
                        const auto arr_size = arr->GetSize();
                        call.Accessor->ClearArray(arg_data);

                        for (int32 j = 0; j < arr_size; j++) {
                            call.Accessor->AddArrayElement(arg_data, arr->At(j));
                        }

                        arr->Release();
                    }
                    else if (arg_type.Kind == ComplexTypeKind::Dict) {
                        const auto* dict = cast_from_void<ScriptDict*>(mutable_data[i]);
                        const auto& dict_map = dict->GetMap();
                        call.Accessor->ClearDict(arg_data);

                        for (const auto& kv : dict_map) {
                            call.Accessor->AddDictElement(arg_data, kv.first, kv.second);
                        }

                        dict->Release();
                    }
                    else {
                        throw NotSupportedException(FO_LINE_STR);
                    }
                }
            }
        }
    }
}

FO_END_NAMESPACE

#endif
