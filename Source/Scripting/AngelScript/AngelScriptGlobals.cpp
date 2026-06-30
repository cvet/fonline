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

template<size_t... I>
[[noreturn]] static void ThrowWithArgs(string_view message, const vector<string>& obj_infos, std::index_sequence<I...> /*unused*/)
{
    throw ScriptException(message, obj_infos[I]...);
}

template<size_t ArgsCount>
static void Global_ThrowException(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& message = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    vector<string> obj_infos;
    obj_infos.reserve(ArgsCount);

    for (int32_t i = 1; i < gen->GetArgCount(); i++) {
        const auto* obj = *static_cast<void**>(gen->GetAddressOfArg(i));
        const auto obj_type_id = gen->GetArgTypeId(i);
        obj_infos.emplace_back(GetScriptObjectInfo(obj, obj_type_id));
    }

    ThrowWithArgs(message, obj_infos, std::make_index_sequence<ArgsCount> {});
}

static void Global_Yield(int32_t durationMs)
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    auto* engine = GetGameEngine(ctx->GetEngine());
    auto* backend = GetScriptBackend(engine);
    auto* context_mngr = backend->GetContextMngr();
    context_mngr->SuspendScriptContext(ctx, engine->GameTime.GetFrameTime() + std::chrono::milliseconds(durationMs));
}

static auto Global_GetGlobalExceptionCount() -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* ctx = AngelScript::asGetActiveContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    const auto* backend = GetScriptBackend(ctx->GetEngine());
    return backend->GetExceptionCounter();
}

static auto Global_GetContextExceptionCount() -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    const auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");
    return ctx_ext->ExceptionCount;
}

static void Global_RunScriptGC()
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    auto* as_engine = ctx->GetEngine();
    as_engine->GarbageCollect(AngelScript::asGC_FULL_CYCLE);
}

static auto ResolveInvokeArgTypes(AngelScript::asIScriptGeneric* gen, AngelScript::asUINT first_arg) -> vector<ComplexTypeDesc>
{
    FO_STACK_TRACE_ENTRY();

    const auto args_count = numeric_cast<size_t>(gen->GetArgCount()) - first_arg;

    if (args_count > MAX_CALL_ARGS) {
        throw ScriptException(strex("Invoke supports at most {} arguments", MAX_CALL_ARGS).str());
    }

    auto* as_engine = gen->GetEngine();
    vector<ComplexTypeDesc> arg_types;
    arg_types.reserve(args_count);

    const auto arg_count = numeric_cast<AngelScript::asUINT>(gen->GetArgCount());

    for (auto arg_index = first_arg; arg_index < arg_count; arg_index++) {
        const auto arg_type_id = gen->GetArgTypeId(arg_index);
        auto arg_type = ResolveScriptFuncType(as_engine, arg_type_id);

        if (!arg_type) {
            throw ScriptException(strex("Unsupported invoke argument type '{}'", as_engine->GetTypeDeclaration(arg_type_id, true)).str());
        }

        arg_types.emplace_back(std::move(arg_type));
    }

    return arg_types;
}

static auto ResolveInvokeResultType(AngelScript::asIScriptGeneric* gen, AngelScript::asUINT result_arg) -> ComplexTypeDesc
{
    FO_STACK_TRACE_ENTRY();

    auto* as_engine = gen->GetEngine();
    const int32_t result_type_id = gen->GetArgTypeId(result_arg);
    auto result_type = ResolveScriptFuncType(as_engine, result_type_id);

    if (!result_type) {
        throw ScriptException(strex("Unsupported invoke result type '{}'", as_engine->GetTypeDeclaration(result_type_id, true)).str());
    }

    return result_type;
}

static auto InvokeResolvedFunction(ScriptFuncDesc* func_desc, AngelScript::asIScriptGeneric* gen, AngelScript::asUINT first_arg, void* ret_data = nullptr) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(func_desc, "Missing script function descriptor");
    FO_VERIFY_AND_THROW(func_desc->Call, "Script function descriptor has no native call handler");

    array<void*, MAX_CALL_ARGS> args_data {};
    array<void*, MAX_CALL_ARGS> indirect_args {};
    const auto args_count = numeric_cast<size_t>(gen->GetArgCount()) - first_arg;

    for (size_t index = 0; index < args_count; index++) {
        auto* arg_data = gen->GetArgAddress(first_arg + numeric_cast<AngelScript::asUINT>(index));
        const auto& arg_type = func_desc->Args[index].Type;
        const bool pass_indirect = arg_type.Kind != ComplexTypeKind::Simple || arg_type.IsMutable || arg_type.BaseType.IsEntity || arg_type.BaseType.IsRefType;

        if (pass_indirect) {
            if (arg_type.Kind == ComplexTypeKind::Simple && !arg_type.BaseType.IsEntity && !arg_type.BaseType.IsRefType) {
                indirect_args[index] = arg_data;
            }
            else {
                indirect_args[index] = MemReadUnaligned<void*>(arg_data);
            }

            args_data[index] = &indirect_args[index];
        }
        else {
            args_data[index] = arg_data;
        }
    }

    FuncCallData call {.Accessor = &SCRIPT_DATA_ACCESSOR};
    call.ArgsData = span(args_data).subspan(0, args_count);
    call.RetData = ret_data;

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

    auto* as_engine = gen->GetEngine();
    const auto* as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(0));

    if (as_type_info == nullptr || as_type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("NameOf: argument must be a function reference");
    }

    const auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(gen->GetArgAddress(0));

    if (func == nullptr) {
        throw ScriptException("NameOf: function reference is null");
    }

    const auto* ns = func->GetNamespace();
    const auto* name = func->GetName();

    string result;

    if (ns != nullptr && ns[0] != '\0') {
        result = string(ns) + "::" + (name != nullptr ? name : "");
    }
    else {
        result = name != nullptr ? string(name) : string();
    }

    new (gen->GetAddressOfReturnLocation()) string(std::move(result));
}

static void Global_InvokeByName(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = GetGameEngine(gen->GetEngine());
    const auto& func_name = *cast_from_void<const string*>(gen->GetAddressOfArg(0));
    const auto hashed_func_name = engine->Hashes.ToHashedString(func_name);
    const auto arg_types = ResolveInvokeArgTypes(gen, 1);
    auto* func_desc = engine->FindFunc(hashed_func_name, span(arg_types));

    if (func_desc == nullptr) {
        throw ScriptException("Script function not found", func_name);
    }

    const bool result = InvokeResolvedFunction(func_desc, gen, 1);
    new (gen->GetAddressOfReturnLocation()) bool(result);
}

static void Global_InvokeByNameWithResult(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = GetGameEngine(gen->GetEngine());
    const auto& func_name = *cast_from_void<const string*>(gen->GetAddressOfArg(0));
    const auto hashed_func_name = engine->Hashes.ToHashedString(func_name);
    const auto result_type = ResolveInvokeResultType(gen, 1);
    const auto arg_types = ResolveInvokeArgTypes(gen, 2);
    auto* func_desc = engine->FindFunc(hashed_func_name, span(arg_types), result_type);

    if (func_desc == nullptr) {
        throw ScriptException("Script function not found", func_name);
    }

    const bool result = InvokeResolvedFunction(func_desc, gen, 2, gen->GetArgAddress(1));
    new (gen->GetAddressOfReturnLocation()) bool(result);
}

static void Global_GetGame(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto* as_engine = gen->GetEngine();
    auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());

    if (!backend->HasGameEngine()) {
        throw ScriptException("Game engine is not available");
    }

    auto* engine = static_cast<Entity*>(backend->GetGameEngine());
    new (gen->GetAddressOfReturnLocation()) Entity*(engine);
}

static void Global_IsGameDestroying(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    // True once the game engine is gone, e.g. in script object destructors during backend destruction (when Game is null)
    const auto* as_engine = gen->GetEngine();
    auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());

    const bool destroying = backend == nullptr || !backend->HasGameEngine();
    new (gen->GetAddressOfReturnLocation()) bool(destroying);
}

static void Global_GetPropertyGroup(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* arr = cast_from_void<ScriptArray*>(gen->GetAuxiliary());
    arr->AddRef();
    new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr);
}

static void Game_ParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto& enum_value_name = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    const int32_t enum_value = meta->ResolveEnumValue(enum_name, enum_value_name);
    new (gen->GetAddressOfReturnLocation()) int32_t(enum_value);
}

static void Game_TryParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto& enum_value_name = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    bool failed = false;
    const int32_t enum_value = meta->ResolveEnumValue(enum_name, enum_value_name, &failed);

    if (!failed) {
        const auto& enum_type = meta->GetBaseType(enum_name);
        MemFill(gen->GetArgAddress(1), 0, enum_type.Size);
        MemCopy(gen->GetArgAddress(1), &enum_value, enum_type.Size);
    }

    new (gen->GetAddressOfReturnLocation()) bool(!failed);
}

static void Game_EnumToString(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    int32_t enum_index = 0;
    MemCopy(&enum_index, gen->GetAddressOfArg(0), meta->GetBaseType(enum_name).Size);
    const bool full_spec = *cast_from_void<bool*>(gen->GetAddressOfArg(1));

    bool failed = false;
    string enum_value_name = meta->ResolveEnumValueName(enum_name, enum_index, &failed);

    if (failed) {
        throw ScriptException("Invalid enum index", enum_name, enum_index);
    }

    if (full_spec) {
        enum_value_name = strex("{}::{}", enum_name, enum_value_name);
    }

    new (gen->GetAddressOfReturnLocation()) string(std::move(enum_value_name));
}

static auto Game_ParseGenericEnum(Entity* entity, string enum_name, string value_name) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* engine = dynamic_cast<BaseEngine*>(entity);
    FO_VERIFY_AND_THROW(engine, "Missing required engine");
    return engine->ResolveEnumValue(enum_name, value_name);
}

template<typename T>
static void Setting_GetEngineValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& value = *cast_from_void<const T*>(gen->GetAuxiliary());
    new (gen->GetAddressOfReturnLocation()) T(value);
}

template<typename T>
static void Setting_SetEngineValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto& value = *cast_from_void<T*>(gen->GetAuxiliary());
    const auto& new_value = *cast_from_void<T*>(gen->GetAddressOfArg(0));
    value = new_value;
}

template<typename T>
struct SettingScalarTypeNames
{
    static constexpr const char* ScalarName = nullptr;
    static constexpr const char* ArrayName = nullptr;
};
template<>
struct SettingScalarTypeNames<int32_t>
{
    static constexpr const char* ScalarName = "int";
    static constexpr const char* ArrayName = "array<int>";
};
template<>
struct SettingScalarTypeNames<int64_t>
{
    static constexpr const char* ScalarName = "int64";
    static constexpr const char* ArrayName = "array<int64>";
};
template<>
struct SettingScalarTypeNames<int8_t>
{
    static constexpr const char* ScalarName = "int8";
    static constexpr const char* ArrayName = "array<int8>";
};
template<>
struct SettingScalarTypeNames<uint8_t>
{
    static constexpr const char* ScalarName = "uint8";
    static constexpr const char* ArrayName = "array<uint8>";
};
template<>
struct SettingScalarTypeNames<int16_t>
{
    static constexpr const char* ScalarName = "int16";
    static constexpr const char* ArrayName = "array<int16>";
};
template<>
struct SettingScalarTypeNames<uint16_t>
{
    static constexpr const char* ScalarName = "uint16";
    static constexpr const char* ArrayName = "array<uint16>";
};
template<>
struct SettingScalarTypeNames<float32_t>
{
    static constexpr const char* ScalarName = "float";
    static constexpr const char* ArrayName = "array<float>";
};
template<>
struct SettingScalarTypeNames<float64_t>
{
    static constexpr const char* ScalarName = "double";
    static constexpr const char* ArrayName = "array<double>";
};
template<>
struct SettingScalarTypeNames<bool>
{
    static constexpr const char* ScalarName = "bool";
    static constexpr const char* ArrayName = "array<bool>";
};
template<>
struct SettingScalarTypeNames<string>
{
    static constexpr const char* ScalarName = "string";
    static constexpr const char* ArrayName = "array<string>";
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

    const auto& vec = *cast_from_void<const vector<T>*>(gen->GetAuxiliary());
    auto* as_engine = gen->GetEngine();

    auto* arr = CreateScriptArray(as_engine, SettingScalarTypeNames<T>::ArrayName);
    arr->Reserve(numeric_cast<int32_t>(vec.size()));

    // Handle vector<bool> in a special way since it has a non-standard reference proxy type.
    for (size_t i = 0; i < vec.size(); i++) {
        T value = vec[i];
        arr->InsertLast(cast_to_void(&value));
    }

    new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr);
}

static void Setting_GetValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto* engine = GetGameEngine(gen->GetEngine());
    const auto& type = engine->GetGameSetting(name);
    const auto& value = engine->Settings.GetCustomSetting(name);

    if (type.IsString) {
        if (type.Name == "any") {
            new (gen->GetAddressOfReturnLocation()) any_t(value);
        }
        else {
            new (gen->GetAddressOfReturnLocation()) string(value);
        }
    }
    else if (type.IsEnum) {
        WriteEnumValueFromInt32(gen->GetAddressOfReturnLocation(), type, numeric_cast<int32_t>(strvex(value).to_int64()));
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
            new (gen->GetAddressOfReturnLocation()) float32_t(strvex(value).to_float32());
        }
        else if (type.IsDoubleFloat) {
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

    const auto& name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto* engine = GetGameEngine(gen->GetEngine());
    const auto& type = engine->GetGameSetting(name);
    const void* new_value = gen->GetAddressOfArg(0);

    string value;

    if (type.IsString) {
        if (type.Name == "any") {
            value = *cast_from_void<const any_t*>(new_value);
        }
        else {
            value = *cast_from_void<const string*>(new_value);
        }
    }
    else if (type.IsEnum) {
        value = strex("{}", ReadEnumValueAsInt32(new_value, type));
    }
    else if (type.IsPrimitive) {
        if (type.IsBool) {
            value = strex("{}", *cast_from_void<const bool*>(new_value));
        }
        else if (type.IsInt8) {
            value = strex("{}", *cast_from_void<const int8_t*>(new_value));
        }
        else if (type.IsInt16) {
            value = strex("{}", *cast_from_void<const int16_t*>(new_value));
        }
        else if (type.IsInt32) {
            value = strex("{}", *cast_from_void<const int32_t*>(new_value));
        }
        else if (type.IsInt64) {
            value = strex("{}", *cast_from_void<const int64_t*>(new_value));
        }
        else if (type.IsUInt8) {
            value = strex("{}", *cast_from_void<const uint8_t*>(new_value));
        }
        else if (type.IsUInt16) {
            value = strex("{}", *cast_from_void<const uint16_t*>(new_value));
        }
        else if (type.IsUInt32) {
            value = strex("{}", *cast_from_void<const uint32_t*>(new_value));
        }
        else if (type.IsUInt64) {
            value = strex("{}", *cast_from_void<const uint64_t*>(new_value));
        }
        else if (type.IsSingleFloat) {
            value = strex("{}", *cast_from_void<const float32_t*>(new_value));
        }
        else if (type.IsDoubleFloat) {
            value = strex("{}", *cast_from_void<const float64_t*>(new_value));
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    engine->Settings.SetCustomSetting(name, any_t(std::move(value)));
}

static void Setting_GetGroup(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    new (gen->GetAddressOfReturnLocation()) void*(gen->GetObject());
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

void RegisterAngelScriptEnums(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    const auto* meta = GetEngineMetadata(as_engine);

    for (auto&& [enum_name, enum_pairs] : meta->GetAllEnums()) {
        const auto& enum_type = meta->GetBaseType(enum_name);
        const auto* underlying_type_name = enum_type.EnumUnderlyingType ? enum_type.EnumUnderlyingType->Name.c_str() : "int32";
        FO_AS_VERIFY(as_engine->RegisterEnum(enum_name.c_str(), underlying_type_name));

        for (auto&& [key, value] : enum_pairs) {
            FO_AS_VERIFY(as_engine->RegisterEnumValue(enum_name.c_str(), key.c_str(), value));
        }
    }
}

void RegisterAngelScriptGlobals(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto* backend = GetScriptBackend(as_engine);
    const auto* meta = backend->GetMetadata();

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
        AngelScript::asIScriptFunction* func = as_engine->GetGlobalFunctionByIndex(i);

        if (func != nullptr && string_view(func->GetName()) == "throw") {
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
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("bool InvokeResult(string funcName, ?&out result, const ?&in ...)", FO_SCRIPT_GENERIC(Global_InvokeByNameWithResult), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("string NameOf(?&in obj)", FO_SCRIPT_GENERIC(Global_NameOf), FO_SCRIPT_GENERIC_CONV));

    // Enum helpers
    for (const auto& enum_name : meta->GetAllEnums() | std::views::keys) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{} ParseEnum_{}(string valueName)", enum_name, enum_name).c_str(), FO_SCRIPT_GENERIC(Game_ParseEnum), FO_SCRIPT_GENERIC_CONV, cast_to_void(&enum_name)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("bool TryParseEnum(string valueName, {}&out result)", enum_name, enum_name).c_str(), FO_SCRIPT_GENERIC(Game_TryParseEnum), FO_SCRIPT_GENERIC_CONV, cast_to_void(&enum_name)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("string EnumToString({} value, bool fullSpecification = false)", enum_name).c_str(), FO_SCRIPT_GENERIC(Game_EnumToString), FO_SCRIPT_GENERIC_CONV, cast_to_void(&enum_name)));
    }

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", "int ParseGenericEnum(string enumName, string valueName)", FO_SCRIPT_FUNC_THIS(Game_ParseGenericEnum), FO_SCRIPT_FUNC_THIS_CONV));

    // Property enum groups
    for (const auto& type_name : meta->GetEntityTypes() | std::views::keys) {
        const auto* registrator = meta->GetPropertyRegistrator(type_name);

        for (auto&& [group_name, properties] : registrator->GetPropertyGroups()) {
            auto* enums_arr = CreateScriptArray(as_engine, strex("array<{}Property>", registrator->GetTypeName()).c_str());
            backend->AddCleanupCallback([enums_arr]() FO_DEFERRED { enums_arr->Release(); });
            enums_arr->Reserve(numeric_cast<int32_t>(properties.size()));

            for (const auto& prop : properties) {
                const int32_t e = prop->GetRegIndex();
                enums_arr->InsertLast(cast_to_void(&e));
            }

            FO_AS_VERIFY(as_engine->SetDefaultNamespace(strex("{}PropertyGroup", registrator->GetTypeName()).c_str()));
            FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("array<{}Property>@+ get_{}()", registrator->GetTypeName(), group_name).c_str(), FO_SCRIPT_GENERIC(Global_GetPropertyGroup), FO_SCRIPT_GENERIC_CONV, cast_to_void(enums_arr)));
            FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));
        }
    }

    // Settings
    FO_AS_VERIFY(as_engine->RegisterObjectType("GlobalSettings", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
    FO_AS_VERIFY(as_engine->RegisterGlobalProperty("GlobalSettings Settings", cast_to_void(meta)));

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
                const auto& parent_type = setting_group_types.at(parent_path);

                FO_AS_VERIFY(as_engine->RegisterObjectType(type_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(parent_type.c_str(), strex("{}@ get_{}() const", type_name, part).c_str(), FO_SCRIPT_GENERIC(Setting_GetGroup), FO_SCRIPT_GENERIC_CONV));

                setting_group_types.emplace(current_path, type_name);
            }

            parent_path = current_path;
        }

        return setting_group_types.at(current_path);
    };

    const auto register_engine_setting = [&]<typename T>(const char* owner_type, const char* name, T& data, bool writeble) {
        if constexpr (SettingScalarTypeNames<T>::ScalarName != nullptr) {
            constexpr const char* type_str = SettingScalarTypeNames<T>::ScalarName;
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type, strex("{} get_{}()", type_str, name).c_str(), FO_SCRIPT_GENERIC(Setting_GetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&data)));

            if (writeble) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type, strex("void set_{}({} value)", name, type_str).c_str(), FO_SCRIPT_GENERIC(Setting_SetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&data)));
            }
        }
        else if constexpr (IsVectorSetting<T>::value) {
            using Elem = typename T::value_type;
            ignore_unused(writeble);

            if constexpr (SettingScalarTypeNames<Elem>::ArrayName != nullptr) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type, strex("{}@ get_{}()", SettingScalarTypeNames<Elem>::ArrayName, name).c_str(), FO_SCRIPT_GENERIC(Setting_GetEngineVectorValue<Elem>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&data)));
            }
        }
    };

    static GlobalSettings dummy_settings(false);
    GlobalSettings& settings = backend->HasGameEngine() ? backend->GetGameEngine()->Settings : dummy_settings;

#define FIXED_SETTING(type, group, name, ...) register_engine_setting.operator()<type>(ensure_setting_group(vector<string> {#group}).c_str(), #name, const_cast<type&>(settings.name), false)
#define VARIABLE_SETTING(type, group, name, ...) register_engine_setting.operator()<type>(ensure_setting_group(vector<string> {#group}).c_str(), #name, settings.name, true)
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

        const auto& accessor_name = path.back();

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type.c_str(), strex("{} get_{}()", MakeScriptTypeName(*setting_type), accessor_name).c_str(), FO_SCRIPT_GENERIC(Setting_GetValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(&setting_name)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(owner_type.c_str(), strex("void set_{}({} value)", accessor_name, MakeScriptTypeName(*setting_type)).c_str(), FO_SCRIPT_GENERIC(Setting_SetValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(&setting_name)));
    }
}

FO_END_NAMESPACE

#endif
