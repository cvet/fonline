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

#include "AngelScriptGlobals.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptAttributes.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptContext.h"
#include "AngelScriptHelpers.h"
#include "Settings.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

static auto LookupScriptBackend(ptr<AngelScript::asIScriptEngine> as_engine) noexcept -> nptr<AngelScriptBackend>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());
}

template<size_t... I>
[[noreturn]] static void ThrowWithArgs(string_view message, const vector<string>& obj_infos, std::index_sequence<I...> /*unused*/)
{
    FO_NO_STACK_TRACE_ENTRY();

    throw ScriptException(message, obj_infos[I]...);
}

template<size_t ArgsCount>
static void Global_ThrowException(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptGeneric> generic = gen;
    auto message = GetGenericAddressArgAs<const string>(generic, 0);

    vector<string> obj_infos;
    obj_infos.reserve(ArgsCount);

    for (AngelScript::asUINT i = 1; i < numeric_cast<AngelScript::asUINT>(generic->GetArgCount()); i++) {
        const auto obj = NativeDataProvider::ReadHandleSlot(GetGenericAddressArg(generic, i));
        const auto obj_type_id = generic->GetArgTypeId(i);
        obj_infos.emplace_back(obj ? GetScriptObjectInfo(obj, obj_type_id) : string {"null"});
    }

    ThrowWithArgs(*message, obj_infos, std::make_index_sequence<ArgsCount> {});
}

static void Global_Yield(int32_t durationMs)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
    auto engine = GetGameEngine(as_engine);
    auto backend = GetScriptBackend(engine);
    auto context_mngr = backend->GetContextMngr();
    FO_VERIFY_AND_THROW(context_mngr, "Missing script context manager");
    context_mngr->SuspendScriptContext(ctx, engine->GameTime.GetFrameTime() + std::chrono::milliseconds(durationMs));
}

static auto Global_GetGlobalExceptionCount() -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    return backend->GetExceptionCounter();
}

static auto Global_GetContextExceptionCount() -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");
    return ctx_ext->ExceptionCount;
}

static void Global_RunScriptGC()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = make_nptr(AngelScript::asGetActiveContext());
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
    as_engine->GarbageCollect(AngelScript::asGC_FULL_CYCLE);
}

static auto ResolveInvokeArgTypes(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT first_arg) -> vector<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto args_count = numeric_cast<size_t>(gen->GetArgCount()) - first_arg;

    if (args_count > MAX_CALL_ARGS) {
        throw ScriptException(strex("Invoke supports at most {} arguments", MAX_CALL_ARGS).str());
    }

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    vector<ComplexTypeDesc> arg_types;
    arg_types.reserve(args_count);

    const auto arg_count = numeric_cast<AngelScript::asUINT>(gen->GetArgCount());

    for (auto arg_index = first_arg; arg_index < arg_count; arg_index++) {
        const auto arg_type_id = gen->GetArgTypeId(arg_index);
        auto arg_type = ResolveScriptFuncType(as_engine, arg_type_id);

        if (!arg_type) {
            const nptr<const char> type_decl = as_engine->GetTypeDeclaration(arg_type_id, true);
            throw ScriptException(strex("Unsupported invoke argument type '{}'", type_decl ? type_decl.get() : "<unknown>").str());
        }

        arg_types.emplace_back(std::move(arg_type));
    }

    return arg_types;
}

static auto InvokeResolvedFunction(ptr<const ScriptFuncDesc> func_desc, ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT first_arg) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(func_desc->Call, "Script function descriptor has no native call handler");

    const auto args_count = numeric_cast<size_t>(gen->GetArgCount()) - first_arg;
    small_vector<ptr<void>, 8> args_data;
    args_data.reserve(args_count);
    array<nptr<void>, MAX_CALL_ARGS> indirect_args {};

    for (size_t index = 0; index < args_count; index++) {
        ptr<void> arg_data = gen->GetArgAddress(first_arg + numeric_cast<AngelScript::asUINT>(index));
        auto arg_type = make_ptr(&func_desc->Args[index].Type);

        // Mutable simple arguments follow the unified slot contract: the slot is the address of the
        // caller's variable (the value itself or the handle cell), which GetArgAddress already returns
        // for the '?&' variadic reference. Non-mutable entity/ref-type and collection arguments are
        // re-packed into a local handle cell so the callee sees a plain handle slot.
        const bool repack_into_handle_cell = arg_type->Kind != ComplexTypeKind::Simple || (!arg_type->IsMutable && (arg_type->BaseType.IsEntity || arg_type->BaseType.IsRefType));

        if (repack_into_handle_cell) {
            indirect_args[index] = MemReadUnaligned<void*>(arg_data);
            args_data.emplace_back(make_ptr(indirect_args[index].get_pp()).void_cast());
        }
        else {
            args_data.emplace_back(arg_data);
        }
    }

    auto accessor = make_ptr(&SCRIPT_DATA_ACCESSOR);
    FuncCallData call {.Accessor = accessor};
    call.ArgsData = const_span<ptr<void>> {args_data.data(), args_data.size()};

    try {
        func_desc->Call(call);
        return true;
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        return false;
    }
}

static void Global_NameOf(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    nptr<const AngelScript::asITypeInfo> as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(0));
    nptr<const AngelScript::asIScriptFunction> funcdef {};

    if (as_type_info) {
        funcdef = as_type_info->GetFuncdefSignature();
    }

    if (!funcdef) {
        throw ScriptException("NameOf: argument must be a function reference");
    }

    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(GetGenericArgAddress(gen, 0));

    if (!func) {
        throw ScriptException("NameOf: function reference is null");
    }

    const nptr<const char> ns = func->GetNamespace();
    const nptr<const char> name = func->GetName();
    const string_view ns_view = ns ? string_view {ns.get()} : string_view {};
    const string_view name_view = name ? string_view {name.get()} : string_view {};

    string result;

    if (!ns_view.empty()) {
        result = string(ns_view);
        result += "::";
        result += name_view;
    }
    else {
        result = string(name_view);
    }

    new (gen->GetAddressOfReturnLocation()) string(std::move(result));
}

static void Global_InvokeByName(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);
    auto func_name = GetGenericAddressArgAs<const string>(gen, 0);
    const auto hashed_func_name = engine->Hashes.ToHashedString(*func_name);
    ptr<AngelScript::asIScriptGeneric> generic = gen;
    const auto arg_types = ResolveInvokeArgTypes(generic, 1);
    auto func_desc = engine->FindFunc(hashed_func_name, span(arg_types));

    if (!func_desc) {
        throw ScriptException("Script function not found", *func_name);
    }

    const bool result = InvokeResolvedFunction(func_desc, generic, 1);
    new (gen->GetAddressOfReturnLocation()) bool(result);
}

static void Global_GetGame(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = LookupScriptBackend(as_engine);

    if (!backend || !backend->HasGameEngine()) {
        throw ScriptException("Game engine is not available");
    }

    ptr<Entity> engine = backend->GetGameEngine();
    ReturnGenericEntity(gen, engine);
}

static void Global_IsGameDestroying(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    // True once the game engine is gone, e.g. in script object destructors during backend destruction (when Game is null)
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = LookupScriptBackend(as_engine);

    const bool destroying = !backend || !backend->HasGameEngine();
    new (gen->GetAddressOfReturnLocation()) bool(destroying);
}

static void Global_GetPropertyGroup(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto arr = GetGenericAuxiliaryAs<ScriptArray>(gen);
    ReturnGenericScriptArray(gen, arr);
}

static void Game_ParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto enum_name = GetGenericAuxiliaryAs<const string>(gen);
    auto enum_value_name = GetGenericAddressArgAs<const string>(gen, 0);

    const int32_t enum_value = meta->ResolveEnumValue(*enum_name, *enum_value_name);
    new (gen->GetAddressOfReturnLocation()) int32_t(enum_value);
}

static void Game_TryParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto enum_name = GetGenericAuxiliaryAs<const string>(gen);
    auto enum_value_name = GetGenericAddressArgAs<const string>(gen, 0);

    bool failed = false;
    const int32_t enum_value = meta->ResolveEnumValue(*enum_name, *enum_value_name, &failed);

    if (!failed) {
        const auto& enum_type = meta->GetBaseType(*enum_name);
        auto result_arg = GetGenericArgAddress(gen, 1);
        MemFill(result_arg, 0, enum_type.Size);
        MemCopy(result_arg, &enum_value, enum_type.Size);
    }

    new (gen->GetAddressOfReturnLocation()) bool(!failed);
}

static void Game_EnumToString(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    auto enum_name = GetGenericAuxiliaryAs<const string>(gen);
    int32_t enum_index = 0;
    const auto& enum_type = meta->GetBaseType(*enum_name);
    auto enum_arg = GetGenericAddressArgAs<const void>(gen, 0);
    MemCopy(&enum_index, enum_arg, enum_type.Size);
    const bool full_spec = *GetGenericAddressArgAs<bool>(gen, 1);

    bool failed = false;
    string enum_value_name {meta->ResolveEnumValueName(*enum_name, enum_index, &failed)};

    if (failed) {
        throw ScriptException("Invalid enum index", *enum_name, enum_index);
    }

    if (full_spec) {
        enum_value_name = strex("{}::{}", *enum_name, enum_value_name);
    }

    new (gen->GetAddressOfReturnLocation()) string(std::move(enum_value_name));
}

static auto Game_ParseGenericEnum(Entity* entity, string enum_name, string value_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    ptr<Entity> entity_ref = entity;
    auto engine = entity_ref.dyn_cast<BaseEngine>();
    FO_VERIFY_AND_THROW(engine, "Missing required engine");
    return engine->ResolveEnumValue(enum_name, value_name);
}

template<typename T>
static void Setting_GetEngineValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto value = GetGenericAuxiliaryAs<const T>(gen);
    new (gen->GetAddressOfReturnLocation()) T(*value);
}

template<typename T>
static void Setting_SetEngineValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto value = GetGenericAuxiliaryAs<T>(gen);
    auto new_value = GetGenericAddressArgAs<const T>(gen, 0);
    *value = *new_value;
}

template<typename T>
struct SettingScalarTypeNames
{
    static constexpr string_view ScalarName {};
    static constexpr string_view ArrayName {};
};
template<>
struct SettingScalarTypeNames<int32_t>
{
    static constexpr string_view ScalarName {"int"};
    static constexpr string_view ArrayName {"array<int>"};
};
template<>
struct SettingScalarTypeNames<int64_t>
{
    static constexpr string_view ScalarName {"int64"};
    static constexpr string_view ArrayName {"array<int64>"};
};
template<>
struct SettingScalarTypeNames<int8_t>
{
    static constexpr string_view ScalarName {"int8"};
    static constexpr string_view ArrayName {"array<int8>"};
};
template<>
struct SettingScalarTypeNames<uint8_t>
{
    static constexpr string_view ScalarName {"uint8"};
    static constexpr string_view ArrayName {"array<uint8>"};
};
template<>
struct SettingScalarTypeNames<int16_t>
{
    static constexpr string_view ScalarName {"int16"};
    static constexpr string_view ArrayName {"array<int16>"};
};
template<>
struct SettingScalarTypeNames<uint16_t>
{
    static constexpr string_view ScalarName {"uint16"};
    static constexpr string_view ArrayName {"array<uint16>"};
};
template<>
struct SettingScalarTypeNames<float32_t>
{
    static constexpr string_view ScalarName {"float"};
    static constexpr string_view ArrayName {"array<float>"};
};
template<>
struct SettingScalarTypeNames<float64_t>
{
    static constexpr string_view ScalarName {"double"};
    static constexpr string_view ArrayName {"array<double>"};
};
template<>
struct SettingScalarTypeNames<bool>
{
    static constexpr string_view ScalarName {"bool"};
    static constexpr string_view ArrayName {"array<bool>"};
};
template<>
struct SettingScalarTypeNames<string>
{
    static constexpr string_view ScalarName {"string"};
    static constexpr string_view ArrayName {"array<string>"};
};

template<typename>
struct IsVectorSetting : std::false_type
{
};
template<typename U>
struct IsVectorSetting<vector<U>> : std::true_type
{
};

template<typename T>
static void Setting_GetEngineVectorValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto vec = GetGenericAuxiliaryAs<const vector<T>>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();

    auto arr = CreateScriptArray(as_engine, SettingScalarTypeNames<T>::ArrayName);
    arr->Reserve(numeric_cast<int32_t>(vec->size()));

    // Handle vector<bool> in a special way since it has a non-standard reference proxy type.
    for (size_t i = 0; i < vec->size(); i++) {
        T value = (*vec)[i];
        arr->InsertLast(make_nptr(&value).void_cast());
    }

    ReturnGenericScriptArray(gen, std::move(arr));
}

static void Setting_GetValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto name = GetGenericAuxiliaryAs<const string>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);
    const auto& type = engine->GetGameSetting(*name);
    const auto& value = engine->Settings->GetCustomSetting(*name);

    if (type.IsString) {
        if (type.Name == "any") {
            new (gen->GetAddressOfReturnLocation()) any_t(value);
        }
        else {
            new (gen->GetAddressOfReturnLocation()) string(value);
        }
    }
    else if (type.IsEnum) {
        ptr<void> return_value = gen->GetAddressOfReturnLocation();
        WriteEnumValueFromInt32(return_value, type, numeric_cast<int32_t>(strvex(value).to_int64()));
    }
    else if (type.IsPrimitive) {
        if (type.IsBool) {
            new (gen->GetAddressOfReturnLocation()) bool(strvex(value).to_bool());
        }
        else if (type.IsInt8) {
            new (gen->GetAddressOfReturnLocation()) int8_t(numeric_cast<int8_t>(strvex(value).to_int64()));
        }
        else if (type.IsInt16) {
            new (gen->GetAddressOfReturnLocation()) int16_t(numeric_cast<int16_t>(strvex(value).to_int64()));
        }
        else if (type.IsInt32) {
            new (gen->GetAddressOfReturnLocation()) int32_t(numeric_cast<int32_t>(strvex(value).to_int64()));
        }
        else if (type.IsInt64) {
            new (gen->GetAddressOfReturnLocation()) int64_t(numeric_cast<int64_t>(strvex(value).to_int64()));
        }
        else if (type.IsUInt8) {
            new (gen->GetAddressOfReturnLocation()) uint8_t(numeric_cast<uint8_t>(strvex(value).to_int64()));
        }
        else if (type.IsUInt16) {
            new (gen->GetAddressOfReturnLocation()) uint16_t(numeric_cast<uint16_t>(strvex(value).to_int64()));
        }
        else if (type.IsUInt32) {
            new (gen->GetAddressOfReturnLocation()) uint32_t(numeric_cast<uint32_t>(strvex(value).to_int64()));
        }
        else if (type.IsUInt64) {
            new (gen->GetAddressOfReturnLocation()) uint64_t(numeric_cast<uint64_t>(strvex(value).to_int64()));
        }
        else if (type.IsSingleFloat) {
            if (strvex(value).is_non_finite_number()) {
                throw ScriptException("String cast to float32 received a non-finite value");
            }

            // The textual check above misses numeric overflow: a value finite in float64 can still
            // become infinity when narrowed to float32, so the parsed result is validated too.
            const float32_t float_value = strvex(value).to_float32();

            if (!std::isfinite(float_value)) {
                throw ScriptException("String cast to float32 received a non-finite value");
            }

            new (gen->GetAddressOfReturnLocation()) float32_t(float_value);
        }
        else if (type.IsDoubleFloat) {
            if (strvex(value).is_non_finite_number()) {
                throw ScriptException("String cast to float64 received a non-finite value");
            }

            new (gen->GetAddressOfReturnLocation()) float64_t(strvex(value).to_float64());
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }
}

static void Setting_SetValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto name = GetGenericAuxiliaryAs<const string>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);
    const auto& type = engine->GetGameSetting(*name);
    auto new_value = GetGenericAddressArgAs<const void>(gen, 0);

    string value;

    if (type.IsString) {
        if (type.Name == "any") {
            value = *GenericValueAs<any_t>(new_value);
        }
        else {
            value = *GenericValueAs<string>(new_value);
        }
    }
    else if (type.IsEnum) {
        value = strex("{}", ReadEnumValueAsInt32(new_value, type));
    }
    else if (type.IsPrimitive) {
        if (type.IsBool) {
            value = strex("{}", *GenericValueAs<bool>(new_value));
        }
        else if (type.IsInt8) {
            value = strex("{}", *GenericValueAs<int8_t>(new_value));
        }
        else if (type.IsInt16) {
            value = strex("{}", *GenericValueAs<int16_t>(new_value));
        }
        else if (type.IsInt32) {
            value = strex("{}", *GenericValueAs<int32_t>(new_value));
        }
        else if (type.IsInt64) {
            value = strex("{}", *GenericValueAs<int64_t>(new_value));
        }
        else if (type.IsUInt8) {
            value = strex("{}", *GenericValueAs<uint8_t>(new_value));
        }
        else if (type.IsUInt16) {
            value = strex("{}", *GenericValueAs<uint16_t>(new_value));
        }
        else if (type.IsUInt32) {
            value = strex("{}", *GenericValueAs<uint32_t>(new_value));
        }
        else if (type.IsUInt64) {
            value = strex("{}", *GenericValueAs<uint64_t>(new_value));
        }
        else if (type.IsSingleFloat) {
            value = strex("{}", *GenericValueAs<float32_t>(new_value));
        }
        else if (type.IsDoubleFloat) {
            value = strex("{}", *GenericValueAs<float64_t>(new_value));
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    engine->Settings->SetCustomSetting(*name, any_t(std::move(value)));
}

static void Setting_GetGroup(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<void> obj = gen->GetObject();
    new (gen->GetAddressOfReturnLocation()) void*(obj.get());
}

static auto SplitSettingPath(string_view setting_name) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> path;
    size_t prev_pos = 0;

    while (prev_pos <= setting_name.size()) {
        const auto pos = setting_name.find('.', prev_pos);

        if (pos == string_view::npos) {
            path.emplace_back(setting_name.substr(prev_pos));
            break;
        }

        path.emplace_back(setting_name.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + 1;
    }

    return path;
}

static auto MakeScriptSettingGroupTypeName(const vector<string>& path) -> string
{
    FO_STACK_TRACE_ENTRY();

    string result = "GlobalSettingsGroup";

    for (const auto& part : path) {
        result += '_';
        result += part;
    }

    return result;
}

void RegisterAngelScriptEnums(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto meta = GetEngineMetadata(as_engine);

    for (auto&& [enum_name, enum_pairs] : meta->GetAllEnums()) {
        const auto& enum_type = meta->GetBaseType(enum_name);
        FO_AS_VERIFY(as_engine->RegisterEnum(enum_name.c_str(), enum_type.EnumUnderlyingType ? enum_type.EnumUnderlyingType->Name.c_str() : "int32"));

        for (auto&& [key, value] : enum_pairs) {
            FO_AS_VERIFY(as_engine->RegisterEnumValue(enum_name.c_str(), key.c_str(), value));
        }
    }
}

void RegisterAngelScriptGlobals(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto backend = GetScriptBackend(as_engine);
    auto meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Missing engine metadata");

    // Global functions
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message)", FO_SCRIPT_GENERIC(Global_ThrowException<0>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1)", FO_SCRIPT_GENERIC(Global_ThrowException<1>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2)", FO_SCRIPT_GENERIC(Global_ThrowException<2>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3)", FO_SCRIPT_GENERIC(Global_ThrowException<3>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", FO_SCRIPT_GENERIC(Global_ThrowException<4>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", FO_SCRIPT_GENERIC(Global_ThrowException<5>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", FO_SCRIPT_GENERIC(Global_ThrowException<6>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", FO_SCRIPT_GENERIC(Global_ThrowException<7>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", FO_SCRIPT_GENERIC(Global_ThrowException<8>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", FO_SCRIPT_GENERIC(Global_ThrowException<9>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void throw(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", FO_SCRIPT_GENERIC(Global_ThrowException<10>), FO_SCRIPT_GENERIC_CONV));

    for (AngelScript::asUINT i = 0, count = as_engine->GetGlobalFunctionCount(); i < count; i++) {
        nptr<AngelScript::asIScriptFunction> func = as_engine->GetGlobalFunctionByIndex(i);

        if (func && string_view(func->GetName()) == "throw") {
            func->SetNoReturn();
        }
    }

    const auto yield_id = as_engine->RegisterGlobalFunction("void Yield(int durationMs)", FO_SCRIPT_FUNC(Global_Yield), FO_SCRIPT_FUNC_CONV);
    FO_AS_VERIFY(yield_id);
    SetFunctionAttributes(as_engine->GetFunctionById(yield_id), {"Async"});

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("int GetGlobalExceptionCount()", FO_SCRIPT_FUNC(Global_GetGlobalExceptionCount), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("int GetContextExceptionCount()", FO_SCRIPT_FUNC(Global_GetContextExceptionCount), FO_SCRIPT_FUNC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void RunScriptGC()", FO_SCRIPT_FUNC(Global_RunScriptGC), FO_SCRIPT_FUNC_CONV));

    // Global instances
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("GameSingleton@ get_Game()", FO_SCRIPT_GENERIC(Global_GetGame), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool get_IsGameDestroying()", FO_SCRIPT_GENERIC(Global_IsGameDestroying), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool Invoke(string funcName, const ?&in ...)", FO_SCRIPT_GENERIC(Global_InvokeByName), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("string NameOf(?&in obj)", FO_SCRIPT_GENERIC(Global_NameOf), FO_SCRIPT_GENERIC_CONV));

    // Enum helpers
    for (const auto& enum_name : meta->GetAllEnums() | std::views::keys) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{} ParseEnum_{}(string valueName)", enum_name, enum_name).c_str(), FO_SCRIPT_GENERIC(Game_ParseEnum), FO_SCRIPT_GENERIC_CONV, make_nptr(&enum_name).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("bool TryParseEnum(string valueName, {}&out result)", enum_name, enum_name).c_str(), FO_SCRIPT_GENERIC(Game_TryParseEnum), FO_SCRIPT_GENERIC_CONV, make_nptr(&enum_name).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("string EnumToString({} value, bool fullSpecification = false)", enum_name).c_str(), FO_SCRIPT_GENERIC(Game_EnumToString), FO_SCRIPT_GENERIC_CONV, make_nptr(&enum_name).void_cast()));
    }

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", "int ParseGenericEnum(string enumName, string valueName)", FO_SCRIPT_FUNC_THIS(Game_ParseGenericEnum), FO_SCRIPT_FUNC_THIS_CONV));

    // Property enum groups
    for (const auto& type_name : meta->GetEntityTypes() | std::views::keys) {
        auto registrator = meta->GetPropertyRegistrator(type_name);
        FO_VERIFY_AND_THROW(registrator, "Missing property registrator for entity type");

        for (auto&& [group_name, properties] : registrator->GetPropertyGroups()) {
            auto enums_arr = CreateScriptArray(as_engine, strex("array<{}Property>", registrator->GetTypeName()).c_str());
            enums_arr->Reserve(numeric_cast<int32_t>(properties.size()));

            for (const auto& prop : properties) {
                const int32_t e = prop->GetRegIndex();
                enums_arr->InsertLast(make_nptr(&e).void_cast());
            }

            FO_AS_VERIFY(as_engine->SetDefaultNamespace(strex("{}PropertyGroup", registrator->GetTypeName()).c_str()));
            FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("array<{}Property>@+ get_{}()", registrator->GetTypeName(), group_name).c_str(), FO_SCRIPT_GENERIC(Global_GetPropertyGroup), FO_SCRIPT_GENERIC_CONV, make_nptr(enums_arr.get()).void_cast()));
            FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));

            // Hand the array's owned reference to the shutdown cleanup so it outlives this scope but is
            // released exactly once at teardown (the `get_*()` accessor returns a borrowed auto-handle).
            backend->AddCleanupCallback([raw = enums_arr.release_ownership()]() FO_DEFERRED { raw->Release(); });
        }
    }

    // Settings
    FO_AS_VERIFY(as_engine->RegisterObjectType("GlobalSettings", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
    FO_AS_VERIFY(as_engine->RegisterGlobalProperty("GlobalSettings Settings", make_nptr(meta.get()).void_cast()));

    unordered_map<string, string> setting_group_types;
    setting_group_types.emplace("", "GlobalSettings");

    const auto ensure_setting_group = [&](const vector<string>& path) -> const string& {
        FO_VERIFY_AND_THROW(!path.empty(), "AngelScript settings group registration received an empty settings path", setting_group_types.size());

        string current_path;
        string parent_path;

        for (const auto& part : path) {
            if (!current_path.empty()) {
                current_path += '.';
            }

            current_path += part;

            if (!setting_group_types.contains(current_path)) {
                const auto partial_path = SplitSettingPath(current_path);
                const auto type_name = MakeScriptSettingGroupTypeName(partial_path);
                const string parent_type = setting_group_types.at(parent_path);

                FO_AS_VERIFY(as_engine->RegisterObjectType(type_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(parent_type.c_str(), strex("{}@ get_{}() const", type_name, part).c_str(), FO_SCRIPT_GENERIC(Setting_GetGroup), FO_SCRIPT_GENERIC_CONV));

                setting_group_types.emplace(current_path, type_name);
            }

            parent_path = current_path;
        }

        return setting_group_types.at(current_path);
    };

    const auto register_engine_setting = [&]<typename T>(string_view owner_type, string_view name, T& data, bool writeble) {
        const string owner_type_str(owner_type);
        const string name_str(name);

        if constexpr (!SettingScalarTypeNames<T>::ScalarName.empty()) {
            constexpr string_view type_str = SettingScalarTypeNames<T>::ScalarName;
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type_str.c_str(), strex("{} get_{}()", type_str, name_str).c_str(), FO_SCRIPT_GENERIC(Setting_GetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, make_nptr(&data).void_cast()));

            if (writeble) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type_str.c_str(), strex("void set_{}({} value)", name_str, type_str).c_str(), FO_SCRIPT_GENERIC(Setting_SetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, make_nptr(&data).void_cast()));
            }
        }
        else if constexpr (IsVectorSetting<T>::value) {
            using Elem = typename T::value_type;
            ignore_unused(writeble);

            if constexpr (!SettingScalarTypeNames<Elem>::ArrayName.empty()) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type_str.c_str(), strex("{}@ get_{}()", SettingScalarTypeNames<Elem>::ArrayName, name_str).c_str(), FO_SCRIPT_GENERIC(Setting_GetEngineVectorValue<Elem>), FO_SCRIPT_GENERIC_CONV, make_nptr(&data).void_cast()));
            }
        }
    };

    static GlobalSettings dummy_settings(false);
    auto settings = backend->HasGameEngine() ? backend->GetGameEngine()->Settings : make_ptr(&dummy_settings);

#define FIXED_SETTING(type, group, name, ...) register_engine_setting.operator()<type>(ensure_setting_group(vector<string> {#group}), #name, const_cast<type&>(settings->name), false)
#define VARIABLE_SETTING(type, group, name, ...) register_engine_setting.operator()<type>(ensure_setting_group(vector<string> {#group}), #name, settings->name, true)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings.inc"

    for (const auto& [setting_name, setting_type] : meta->GetGameSettings()) {
        const auto path = SplitSettingPath(setting_name);
        FO_VERIFY_AND_THROW(!path.empty(), "AngelScript game setting registration received an empty setting path", setting_name, setting_type->Name);

        string owner_type = "GlobalSettings";

        if (path.size() > 1) {
            const vector<string> group_path(path.begin(), path.end() - 1);
            owner_type = ensure_setting_group(group_path);
        }

        const string_view accessor_name = path.back();

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type.c_str(), strex("{} get_{}()", MakeScriptTypeName(*setting_type), accessor_name).c_str(), FO_SCRIPT_GENERIC(Setting_GetValue), FO_SCRIPT_GENERIC_CONV, make_nptr(&setting_name).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type.c_str(), strex("void set_{}({} value)", accessor_name, MakeScriptTypeName(*setting_type)).c_str(), FO_SCRIPT_GENERIC(Setting_SetValue), FO_SCRIPT_GENERIC_CONV, make_nptr(&setting_name).void_cast()));
    }
}

FO_END_NAMESPACE

#endif
