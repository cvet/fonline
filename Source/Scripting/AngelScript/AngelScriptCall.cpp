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
#include "AngelScriptAttributes.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

auto ScriptDataAccessor::GetArraySize(ptr<void> data) const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(data);
    return arr ? numeric_cast<size_t>(arr->GetSize()) : 0;
}

auto ScriptDataAccessor::GetArrayElement(ptr<void> data, size_t index) const -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(data);
    FO_VERIFY_AND_THROW(arr, "Missing AngelScript array");
    return arr->At(numeric_cast<int32_t>(index));
}

auto ScriptDataAccessor::GetDictSize(ptr<void> data) const -> size_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(data);
    return dict ? numeric_cast<size_t>(dict->GetSize()) : 0;
}

auto ScriptDataAccessor::GetDictElement(ptr<void> data, size_t index) const -> pair<ptr<void>, ptr<void>>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(data);
    FO_VERIFY_AND_THROW(dict, "Missing AngelScript dictionary");
    const auto it = std::next(dict->GetMap()->begin(), static_cast<ptrdiff_t>(index));
    return pair<ptr<void>, ptr<void>>(it->first, it->second);
}

auto ScriptDataAccessor::GetCallback(ptr<void> data) const -> unique_del_nptr<ScriptFuncDesc>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(data);

    if (func) {
        auto func_desc = IndexScriptFunc(func);
        FO_VERIFY_AND_THROW(func_desc->Call, "Script function descriptor has no native call handler");
        return MakeAngelScriptFuncDescBorrow(func_desc, refcount_ptr<AngelScript::asIScriptFunction>::from_add_ref(func.get()));
    }

    return nullptr;
}

void ScriptDataAccessor::ClearArray(ptr<void> data) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(data);
    FO_VERIFY_AND_THROW(arr, "Missing AngelScript array");
    arr->Resize(0);
}

void ScriptDataAccessor::AddArrayElement(ptr<void> data, ptr<void> value) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arr = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(data);
    FO_VERIFY_AND_THROW(arr, "Missing AngelScript array");
    arr->InsertLast(value);
}

void ScriptDataAccessor::ClearDict(ptr<void> data) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(data);
    FO_VERIFY_AND_THROW(dict, "Missing AngelScript dictionary");
    dict->Clear();
}

void ScriptDataAccessor::AddDictElement(ptr<void> data, ptr<void> key, ptr<void> value) const
{
    FO_NO_STACK_TRACE_ENTRY();

    auto dict = NativeDataProvider::ReadTypedHandleSlot<ScriptDict>(data);
    FO_VERIFY_AND_THROW(dict, "Missing AngelScript dictionary");
    dict->Set(key.get(), value.get());
}

auto ResolveScriptFuncType(ptr<AngelScript::asIScriptEngine> as_engine, int32_t type_id, uint32_t flags, bool is_ret) -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(type_id != AngelScript::asTYPEID_VOID, "AngelScript type id unexpectedly resolves to void", type_id);

    auto meta = GetEngineMetadata(as_engine);

    const auto get_type_name = [as_engine](int32_t tid) -> string_view {
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

        nptr<AngelScript::asITypeInfo> ti = as_engine->GetTypeInfoById(tid);
        FO_VERIFY_AND_THROW(ti, "Type info is null");
        return ti->GetName();
    };

    nptr<AngelScript::asITypeInfo> as_type_info = as_engine->GetTypeInfoById(type_id);
    ComplexTypeDesc type;

    if (as_type_info && string_view(as_type_info->GetName()) == "dict") {
        const auto key_name = get_type_name(as_type_info->GetSubTypeId(0));
        const auto value_name = get_type_name(as_type_info->GetSubTypeId(1));

        if (!meta->IsValidBaseType(key_name)) {
            return {};
        }

        if (string_view(value_name) == "array") {
            nptr<AngelScript::asITypeInfo> value_type_info = as_type_info->GetSubType(1);
            FO_VERIFY_AND_THROW(value_type_info, "Missing dictionary value type info");
            const auto value_name2 = get_type_name(value_type_info->GetSubTypeId());

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
    else if (as_type_info && string_view(as_type_info->GetName()) == "array") {
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

    FO_VERIFY_AND_THROW(type, "Missing type descriptor");
    return type;
}

auto IndexScriptFunc(ptr<AngelScript::asIScriptFunction> func) -> ptr<ScriptFuncDesc>
{
    FO_STACK_TRACE_ENTRY();

    if (auto func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData())) {
        return func_desc;
    }

    ptr<AngelScript::asIScriptEngine> as_engine = func->GetEngine();
    auto meta = GetEngineMetadata(as_engine);

    const auto func_name = GetScriptFuncName(func, meta->Hashes);
    auto func_desc = SafeAlloc::MakeUnique<ScriptFuncDesc>();

    func_desc->Name = func_name;
    func_desc->AttributeChecker = [func](string_view attribute) -> bool { return HasFunctionAttribute(func, attribute); };
    func_desc->DelegateObj = std::bit_cast<uintptr_t>(func->GetDelegateObject());

    for (AngelScript::asUINT p = 0; p < func->GetParamCount(); p++) {
        int32_t as_result = 0;
        int32_t param_type_id = 0;
        AngelScript::asDWORD param_flags = 0;
        nptr<const char> param_name;
        FO_AS_VERIFY(func->GetParam(p, &param_type_id, &param_flags, param_name.get_pp()));

        auto arg_type = ResolveScriptFuncType(as_engine, param_type_id, param_flags, false);
        func_desc->Args.emplace_back(ArgDesc {.Name = param_name ? param_name.get() : "", .Type = std::move(arg_type)});
    }

    AngelScript::asDWORD ret_flags = 0;
    const int32_t ret_type_id = func->GetReturnTypeId(&ret_flags);
    const bool is_void_ret = ret_type_id == AngelScript::asTYPEID_VOID;

    if (!is_void_ret) {
        func_desc->Ret = ResolveScriptFuncType(as_engine, ret_type_id, ret_flags, true);

        if (func_desc->Ret.Kind == ComplexTypeKind::Simple) {
            nptr<AngelScript::asITypeInfo> as_ret_type = as_engine->GetTypeInfoById(ret_type_id);

            if (as_ret_type && (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0) {
                func_desc->ReturnValueCleaner = [as_engine, as_ret_type](ptr<void> ret_data) mutable {
                    auto cur_obj = NativeDataProvider::ReadHandleSlot(ret_data);

                    if (cur_obj) {
                        as_engine->ReleaseScriptObject(cur_obj.get(), as_ret_type.get());
                        NativeDataProvider::WriteHandleSlot(ret_data, nullptr);
                    }
                };
            }
        }
    }

    const bool call_supported = (is_void_ret || !!func_desc->Ret) && !std::ranges::any_of(func_desc->Args, [](auto&& a) { return !a.Type; });

    if (call_supported) {
        func_desc->Call = [func](FuncCallData& call) FO_DEFERRED { ScriptFuncCall(func, call); };
    }

    auto released_func_desc = func_desc.release();
    func->SetUserData(released_func_desc.void_cast());
    auto stored_func_desc = cast_from_void<ScriptFuncDesc*>(func->GetUserData());
    FO_VERIFY_AND_THROW(stored_func_desc, "Script function descriptor was not stored");
    return stored_func_desc;
}

// AngelScript passes reference parameters as a pointer on the stack: GetAddressOfArg() returns the
// address of that stack slot (T**), while the unified FuncCallData slot contract (shared with the
// managed backend and consumed by NativeDataCaller) is "address of the caller's variable" - the value
// itself for primitives/enums/value types (T*) and the handle cell for object handles ('Type@&').
// Mutability comes from the registration-time descriptor, which produced the declaration in the first
// place (MakeScriptArgName emits '&'/'@&' exactly for mutable args), so no per-call type interrogation
// is needed: mutable args resolve through GetArgAddress(), which returns the pointer held on the stack.
static auto GetGenericArgSlot(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT arg_index, const ComplexTypeDesc& arg_type) -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (arg_type.IsMutable) {
        auto arg_address = GetGenericArgAddress(gen, arg_index);
        FO_VERIFY_AND_THROW(arg_address, "Reference argument address is null");
        return arg_address;
    }

    return GetGenericAddressArg(gen, arg_index);
}

void ScriptGenericCall(ptr<AngelScript::asIScriptGeneric> gen, bool add_obj, const_span<ArgDesc> args_desc, const function<void(FuncCallData&)>& callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(numeric_cast<size_t>(gen->GetArgCount()) == args_desc.size(), "Generic call argument count does not match descriptor", gen->GetArgCount(), args_desc.size());

    int32_t as_result = 0;
    auto accessor = make_ptr(&SCRIPT_DATA_ACCESSOR);
    FuncCallData call {.Accessor = accessor};
    const size_t args_count = gen->GetArgCount() + (add_obj ? 1 : 0);
    small_vector<ptr<void>, 16> args_data;
    args_data.reserve(args_count);
    array<nptr<void>, 1> this_obj_storage {};

    if (add_obj) {
        auto this_obj = GetGenericObject(gen);
        this_obj_storage[0] = this_obj;
        auto this_obj_slot = GetNullableHandleSlotAddress(&this_obj_storage[0]);

        for (AngelScript::asUINT index = 0; index < args_count; index++) {
            args_data.emplace_back(index == 0 ? this_obj_slot : GetGenericArgSlot(gen, index - 1, args_desc[index - 1].Type));
        }
    }
    else {
        for (AngelScript::asUINT index = 0; index < args_count; index++) {
            args_data.emplace_back(GetGenericArgSlot(gen, index, args_desc[index].Type));
        }
    }

    call.ArgsData = const_span<ptr<void>> {args_data.data(), args_data.size()};

    const auto ret_type_id = gen->GetReturnTypeId();

    if (ret_type_id != AngelScript::asTYPEID_VOID) {
        nptr<void> ret_location = gen->GetAddressOfReturnLocation();
        FO_STRONG_ASSERT(ret_location, "Generic call return location is null");
        call.RetData = ret_location;
        ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();

        if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            nptr<AngelScript::asITypeInfo> as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
            FO_VERIFY_AND_THROW(as_ret_type, "Missing AngelScript return type info");
            const bool is_ref_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0;
            const bool is_value_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_VALUE) != 0;
            const bool is_array = is_ref_type && as_ret_type->GetName() == string_view("array");
            const bool is_dict = is_ref_type && as_ret_type->GetName() == string_view("dict");

            if (is_array || is_dict) {
                nptr<void> ret_obj = as_engine->CreateScriptObject(as_ret_type.get());
                FO_VERIFY_AND_THROW(ret_obj, "Missing AngelScript return object");
                NativeDataProvider::WriteHandleSlot(call.RetData, ret_obj);
            }
            else if (is_ref_type) {
                NativeDataProvider::WriteHandleSlot(call.RetData, nullptr);
            }
            else if (is_value_type) {
                nptr<void> ret_obj = as_engine->CreateScriptObject(as_ret_type.get());
                FO_VERIFY_AND_THROW(ret_obj, "Missing AngelScript return object");
                FO_AS_VERIFY(gen->SetReturnObject(ret_obj.get()));
                as_engine->ReleaseScriptObject(ret_obj.get(), as_ret_type.get());
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
            FO_VERIFY_AND_THROW(call.RetData, "Missing required call ret data");
            ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
            nptr<AngelScript::asITypeInfo> as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
            FO_VERIFY_AND_THROW(as_ret_type, "Missing AngelScript return type info");
            const bool is_ref_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0;
            nptr<void> ret_obj = call.RetData;

            if (is_ref_type) {
                ret_obj = NativeDataProvider::ReadHandleSlot(call.RetData);
            }

            if (ret_obj) {
                as_engine->ReleaseScriptObject(ret_obj.get(), as_ret_type.get());

                if (is_ref_type) {
                    NativeDataProvider::WriteHandleSlot(call.RetData, nullptr);
                }
            }
        }

        throw;
    }
}

void ScriptFuncCall(ptr<AngelScript::asIScriptFunction> func, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(call.ArgsData.size() == func->GetParamCount(), "Script function call argument storage does not match function signature", func->GetDeclaration(), call.ArgsData.size(), func->GetParamCount());
    FO_VERIFY_AND_THROW((call.RetData != nullptr) == (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID), "Script call return storage does not match function return type", call.RetData != nullptr, func->GetReturnTypeId());

    int32_t as_result = 0;
    ptr<AngelScript::asIScriptEngine> as_engine = func->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    auto context_mngr = backend->GetContextMngr();
    FO_VERIFY_AND_THROW(context_mngr, "Missing script context manager");
    auto ctx = context_mngr->PrepareContext(func);
    const uint64_t ctx_generation = context_mngr->GetContextGeneration(ctx);
    auto return_ctx = scope_exit([&, ctx_generation]() noexcept { context_mngr->ReturnContext(ctx, ctx_generation); });
    auto func_desc = cast_from_void<const ScriptFuncDesc*>(func->GetUserData());
    FO_VERIFY_AND_THROW(func_desc, "Missing script function descriptor");
    FO_VERIFY_AND_THROW(func_desc->Call, "Script function descriptor has no native call handler");

    // Check for destroyed or non-synced entity
    bool any_destroyed = false;

    const auto check_entity = [&](nptr<const Entity> entity) {
        if (entity) {
            if (entity->IsDestroyed()) {
                any_destroyed = true;
                return;
            }

            ValidateEntityAccess(entity);
        }
    };

    for (AngelScript::asUINT i = 0; i < call.ArgsData.size() && !any_destroyed; i++) {
        auto arg_type = make_ptr(&func_desc->Args[i].Type);

        if (arg_type->BaseType.IsEntity) {
            if (arg_type->Kind == ComplexTypeKind::Simple) {
                check_entity(NativeDataProvider::ReadTypedHandleSlot<Entity>(call.ArgsData[i]));
            }
            else if (arg_type->Kind == ComplexTypeKind::Array) {
                auto arr_data = call.ArgsData[i];
                const size_t arr_size = call.Accessor->GetArraySize(arr_data);

                for (size_t j = 0; j < arr_size && !any_destroyed; j++) {
                    check_entity(NativeDataProvider::ReadTypedHandleSlot<Entity>(call.Accessor->GetArrayElement(arr_data, j)));
                }
            }
            else if (arg_type->Kind == ComplexTypeKind::DictOfArray) {
                auto dict_data = call.ArgsData[i];
                const size_t dict_size = call.Accessor->GetDictSize(dict_data);

                for (size_t j = 0; j < dict_size && !any_destroyed; j++) {
                    const auto kv = call.Accessor->GetDictElement(dict_data, j);
                    auto array_data = kv.second;
                    const size_t inner_size = call.Accessor->GetArraySize(array_data);

                    for (size_t k = 0; k < inner_size && !any_destroyed; k++) {
                        check_entity(NativeDataProvider::ReadTypedHandleSlot<Entity>(call.Accessor->GetArrayElement(array_data, k)));
                    }
                }
            }
            else if (arg_type->Kind == ComplexTypeKind::Dict) {
                auto dict_data = call.ArgsData[i];
                const size_t dict_size = call.Accessor->GetDictSize(dict_data);

                for (size_t j = 0; j < dict_size && !any_destroyed; j++) {
                    const auto kv = call.Accessor->GetDictElement(dict_data, j);
                    check_entity(NativeDataProvider::ReadTypedHandleSlot<Entity>(kv.second));
                }
            }
        }
    }

    if (any_destroyed) {
        return;
    }

    if (call.Accessor->GetBackendIndex() == ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX) {
        // Direct call AS to AS
        for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
            auto arg_type = make_ptr(&func_desc->Args[i].Type);
            auto base_type = make_ptr(&arg_type->BaseType);
            auto arg_data = call.ArgsData[i];

            if (arg_type->Kind == ComplexTypeKind::Simple) {
                if (arg_type->IsMutable) {
                    // Unified mutable slot: the slot already is the address of the caller's variable
                    // (the value itself or the handle cell), so it is the reference address as-is
                    FO_AS_VERIFY(ctx->SetArgAddress(i, arg_data.get()));
                }
                else if (base_type->IsEntity || base_type->IsRefType) {
                    SetScriptArgObjectFromHandleSlot(ctx, i, arg_data);
                }
                else if (base_type->IsObject) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, arg_data.get()));
                }
                else if (base_type->IsEnum || base_type->IsPrimitive) {
                    auto arg_dest = GetContextAddressOfArg(ctx, i);
                    MemCopy(arg_dest, arg_data, base_type->Size);
                }
                else {
                    throw NotSupportedException("Invalid script func call - invalid simple type", base_type->Name);
                }
            }
            else {
                SetScriptArgObjectFromHandleSlot(ctx, i, arg_data);
            }
        }

        if (context_mngr->RunContext(ctx, !func_desc->Ret)) {
            if (func_desc->Ret) {
                FO_VERIFY_AND_THROW(call.RetData, "Missing required call ret data");
                const auto ret_type_id = func->GetReturnTypeId();

                if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
                    nptr<AngelScript::asITypeInfo> as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
                    FO_VERIFY_AND_THROW(as_ret_type, "Missing AngelScript return type info");
                    const bool is_ref_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0;
                    nptr<void> ret_obj = ctx->GetReturnObject();

                    if (is_ref_type) {
                        nptr<void> cur_obj = NativeDataProvider::ReadHandleSlot(call.RetData);

                        if (cur_obj) {
                            as_engine->ReleaseScriptObject(cur_obj.get(), as_ret_type.get());
                        }

                        NativeDataProvider::WriteHandleSlot(call.RetData, ret_obj);
                        as_engine->AddRefScriptObject(ret_obj.get(), as_ret_type.get());
                    }
                    else {
                        FO_VERIFY_AND_THROW(ret_obj, "Missing AngelScript return object");
                        FO_AS_VERIFY(as_engine->AssignScriptObject(call.RetData.get(), ret_obj.get(), as_ret_type.get()));
                    }
                }
                else {
                    auto ret_value = GetContextAddressOfReturnValue(ctx);
                    MemCopy(call.RetData, ret_value, func_desc->Ret.BaseType.Size);
                }
            }
        }
    }
    else {
        array<nptr<void>, MAX_CALL_ARGS> mutable_data = {};
        array<int32_t, MAX_CALL_ARGS> enum_mutable_data = {};

        for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
            auto arg_type = make_ptr(&func_desc->Args[i].Type);
            auto base_type = make_ptr(&arg_type->BaseType);
            auto arg_data = call.ArgsData[i];

            if (arg_type->Kind == ComplexTypeKind::Simple) {
                if (arg_type->IsMutable) {
                    if (base_type->IsEnum && base_type->EnumUnderlyingType->Size != sizeof(int32_t)) {
                        mutable_data[i] = make_nptr(&enum_mutable_data[i]).void_cast();
                        MemCopy(mutable_data[i], arg_data, base_type->Size);
                        FO_AS_VERIFY(ctx->SetArgAddress(i, mutable_data[i].get()));
                    }
                    else {
                        FO_AS_VERIFY(ctx->SetArgAddress(i, arg_data.get()));
                    }
                }
                else if (base_type->IsEntity || base_type->IsRefType) {
                    SetScriptArgObjectFromHandleSlot(ctx, i, arg_data);
                }
                else if (base_type->IsObject) {
                    FO_AS_VERIFY(ctx->SetArgObject(i, arg_data.get()));
                }
                else if (base_type->IsEnum) {
                    auto arg_dest = GetContextAddressOfArg(ctx, i);
                    MemCopy(arg_dest, arg_data, base_type->Size);
                }
                else if (base_type->IsPrimitive) {
                    auto arg_dest = GetContextAddressOfArg(ctx, i);
                    MemCopy(arg_dest, arg_data, base_type->Size);
                }
                else {
                    throw NotSupportedException("Invalid script func call - invalid simple type", base_type->Name);
                }
            }
            else if (arg_type->Kind == ComplexTypeKind::Array) {
                const size_t arr_size = call.Accessor->GetArraySize(arg_data);
                auto arr = CreateScriptArray(as_engine, MakeScriptTypeName(*arg_type).c_str());
                arr->Reserve(numeric_cast<int32_t>(arr_size));

                for (size_t j = 0; j < arr_size; j++) {
                    auto elem = call.Accessor->GetArrayElement(arg_data, j);
                    arr->InsertLast(elem);
                }

                if (arg_type->IsMutable) {
                    mutable_data[i] = arr;
                    FO_AS_VERIFY(ctx->SetArgAddress(i, GetNullableHandleSlotAddress(&mutable_data[i]).get()));
                    (void)arr.release_ownership();
                }
                else {
                    FO_AS_VERIFY(ctx->SetArgObject(i, arr.get()));
                }
            }
            else if (arg_type->Kind == ComplexTypeKind::Dict) {
                const size_t dict_size = call.Accessor->GetDictSize(arg_data);
                auto dict = CreateScriptDict(as_engine, MakeScriptTypeName(*arg_type).c_str());

                for (size_t j = 0; j < dict_size; j++) {
                    const auto elem = call.Accessor->GetDictElement(arg_data, j);
                    dict->Set(elem.first, elem.second);
                }

                if (arg_type->IsMutable) {
                    mutable_data[i] = dict;
                    FO_AS_VERIFY(ctx->SetArgAddress(i, GetNullableHandleSlotAddress(&mutable_data[i]).get()));
                    (void)dict.release_ownership();
                }
                else {
                    FO_AS_VERIFY(ctx->SetArgObject(i, dict.get()));
                }
            }
            else {
                throw NotSupportedException(FO_LINE_STR);
            }
        }

        if (context_mngr->RunContext(ctx, !func_desc->Ret)) {
            if (func_desc->Ret) {
                FO_VERIFY_AND_THROW(call.RetData, "Missing required call ret data");
                const auto ret_type_id = func->GetReturnTypeId();

                if ((ret_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
                    nptr<AngelScript::asITypeInfo> as_ret_type = as_engine->GetTypeInfoById(ret_type_id);
                    FO_VERIFY_AND_THROW(as_ret_type, "Missing AngelScript return type info");
                    const bool is_ref_type = (as_ret_type->GetFlags() & AngelScript::asOBJ_REF) != 0;
                    nptr<void> ret_obj = ctx->GetReturnObject();

                    if (is_ref_type && func_desc->Ret.Kind == ComplexTypeKind::Array) {
                        auto arr = cast_from_void<ScriptArray*>(ret_obj.get());
                        FO_VERIFY_AND_THROW(arr, "Returned AngelScript array handle is null");
                        const int32_t arr_size = arr->GetSize();
                        call.Accessor->ClearArray(call.RetData);

                        for (int32_t j = 0; j < arr_size; j++) {
                            call.Accessor->AddArrayElement(call.RetData, arr->At(j));
                        }
                    }
                    else if (is_ref_type && (func_desc->Ret.Kind == ComplexTypeKind::Dict || func_desc->Ret.Kind == ComplexTypeKind::DictOfArray)) {
                        auto dict = cast_from_void<ScriptDict*>(ret_obj.get());
                        FO_VERIFY_AND_THROW(dict, "Returned AngelScript dictionary handle is null");
                        ptr<const map<void*, void*, ScriptDict::ScriptDictComparator>> dict_map = dict->GetMap();
                        call.Accessor->ClearDict(call.RetData);

                        for (const auto& kv : *dict_map) {
                            call.Accessor->AddDictElement(call.RetData, kv.first, kv.second);
                        }
                    }
                    else if (is_ref_type) {
                        auto cur_obj = NativeDataProvider::ReadHandleSlot(call.RetData);

                        if (cur_obj) {
                            as_engine->ReleaseScriptObject(cur_obj.get(), as_ret_type.get());
                        }

                        NativeDataProvider::WriteHandleSlot(call.RetData, ret_obj);

                        if (ret_obj) {
                            as_engine->AddRefScriptObject(ret_obj.get(), as_ret_type.get());

                            if (call.Accessor->GetBackendIndex() == ScriptSystemBackend::MANAGED_BACKEND_INDEX) {
                                call.RetValueOwner.Reset([as_engine, as_ret_type, ret_obj]() mutable noexcept {
                                    as_engine->ReleaseScriptObject(ret_obj.get_no_const(), as_ret_type.get());
                                });
                            }
                        }
                    }
                    else {
                        FO_VERIFY_AND_THROW(ret_obj, "Missing AngelScript return object");
                        FO_AS_VERIFY(as_engine->AssignScriptObject(call.RetData.get(), ret_obj.get(), as_ret_type.get()));
                    }
                }
                else {
                    FO_VERIFY_AND_THROW(as_engine->GetSizeOfPrimitiveType(ret_type_id) == numeric_cast<int32_t>(func_desc->Ret.BaseType.Size), "AngelScript primitive return size does not match registered function descriptor", as_engine->GetSizeOfPrimitiveType(ret_type_id), func_desc->Ret.BaseType.Size);
                    auto ret_value = GetContextAddressOfReturnValue(ctx);
                    MemCopy(call.RetData, ret_value, func_desc->Ret.BaseType.Size);
                }
            }

            for (AngelScript::asUINT i = 0; i < call.ArgsData.size(); i++) {
                const nptr<void> mutable_entry = mutable_data[i];

                if (mutable_entry) {
                    auto arg_type = make_ptr(&func_desc->Args[i].Type);
                    auto arg_data = call.ArgsData[i];
                    FO_VERIFY_AND_THROW(arg_type->IsMutable, "Argument type is not mutable");

                    if (arg_type->Kind == ComplexTypeKind::Simple) {
                        auto base_type = make_ptr(&arg_type->BaseType);
                        FO_VERIFY_AND_THROW(base_type->IsEnum, "Mutable AngelScript argument base type is not enum");
                        MemCopy(arg_data, mutable_entry, base_type->Size);
                    }
                    else if (arg_type->Kind == ComplexTypeKind::Array) {
                        auto arr = cast_from_void<ScriptArray*>(mutable_entry);
                        FO_VERIFY_AND_THROW(arr, "Mutable argument array handle is null");
                        const int32_t arr_size = arr->GetSize();
                        call.Accessor->ClearArray(arg_data);

                        for (int32_t j = 0; j < arr_size; j++) {
                            call.Accessor->AddArrayElement(arg_data, arr->At(j));
                        }

                        arr->Release();
                    }
                    else if (arg_type->Kind == ComplexTypeKind::Dict) {
                        auto dict = cast_from_void<ScriptDict*>(mutable_entry);
                        FO_VERIFY_AND_THROW(dict, "Mutable argument dictionary handle is null");
                        auto dict_map = dict->GetMap();
                        call.Accessor->ClearDict(arg_data);

                        for (const auto& kv : *dict_map) {
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
