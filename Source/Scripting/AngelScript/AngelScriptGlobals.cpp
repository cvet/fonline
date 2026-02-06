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

#include "AngelScriptBackend.h"
#include "AngelScriptContext.h"
#include "AngelScriptHelpers.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

template<size_t... I>
[[noreturn]] static void ThrowWithArgs(string_view message, const vector<string>& obj_infos, std::index_sequence<I...> /*unused*/)
{
    throw ScriptException(message, obj_infos[I]...);
}

template<size_t ArgsCount>
static void Global_Assert(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto condition = *cast_from_void<bool*>(gen->GetAddressOfArg(0));

    if (condition) {
        return;
    }

    vector<string> obj_infos;
    obj_infos.reserve(ArgsCount);

    for (int32 i = 1; i < gen->GetArgCount(); i++) {
        const auto* obj = *static_cast<void**>(gen->GetAddressOfArg(i));
        const auto obj_type_id = gen->GetArgTypeId(i);
        obj_infos.emplace_back(GetScriptObjectInfo(obj, obj_type_id));
    }

    ThrowWithArgs("Assertion failed", obj_infos, std::make_index_sequence<ArgsCount> {});
}

template<size_t ArgsCount>
static void Global_ThrowException(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& message = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    vector<string> obj_infos;
    obj_infos.reserve(ArgsCount);

    for (int32 i = 1; i < gen->GetArgCount(); i++) {
        const auto* obj = *static_cast<void**>(gen->GetAddressOfArg(i));
        const auto obj_type_id = gen->GetArgTypeId(i);
        obj_infos.emplace_back(GetScriptObjectInfo(obj, obj_type_id));
    }

    ThrowWithArgs(message, obj_infos, std::make_index_sequence<ArgsCount> {});
}

static void Global_Yield(int32 durationMs)
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = AngelScript::asGetActiveContext();
    FO_RUNTIME_ASSERT(ctx);
    auto* engine = GetGameEngine(ctx->GetEngine());
    auto* backend = GetScriptBackend(engine);
    auto* context_mngr = backend->GetContextMngr();
    context_mngr->SuspendScriptContext(ctx, engine->GameTime.GetFrameTime() + std::chrono::milliseconds(durationMs));
}

static void Global_GetGame(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    // Maybe called in script object destructors during backend destruction
    const auto* as_engine = gen->GetEngine();
    auto* backend = cast_from_void<AngelScriptBackend*>(as_engine->GetUserData());

    if (backend != nullptr && backend->HasGameEngine()) {
        auto* engine = static_cast<Entity*>(backend->GetGameEngine());
        new (gen->GetAddressOfReturnLocation()) Entity*(engine);
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
    }
}

static void Global_GetPropertyGroup(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* arr = cast_from_void<ScriptArray*>(gen->GetAuxiliary());
    new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr);
}

static void Game_ParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto& enum_value_name = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    const int32 enum_value = meta->ResolveEnumValue(enum_name, enum_value_name);
    new (gen->GetAddressOfReturnLocation()) int32(enum_value);
}

static void Game_TryParseEnum(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto& enum_value_name = *cast_from_void<string*>(gen->GetAddressOfArg(0));

    bool failed = false;
    const int32 enum_value = meta->ResolveEnumValue(enum_name, enum_value_name, &failed);

    if (!failed) {
        *cast_from_void<int32*>(gen->GetArgAddress(1)) = enum_value;
    }

    new (gen->GetAddressOfReturnLocation()) bool(!failed);
}

static void Game_EnumToString(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* meta = GetEngineMetadata(gen->GetEngine());
    const auto& enum_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto enum_index = *cast_from_void<int32*>(gen->GetAddressOfArg(0));
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

static auto Game_ParseGenericEnum(Entity* entity, string enum_name, string value_name) -> int32
{
    FO_STACK_TRACE_ENTRY();

    const auto* engine = dynamic_cast<BaseEngine*>(entity);
    FO_RUNTIME_ASSERT(engine);
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
        new (gen->GetAddressOfReturnLocation()) int32(numeric_cast<int32>(strvex(value).to_int64()));
    }
    else if (type.IsPrimitive) {
        if (type.IsBool) {
            new (gen->GetAddressOfReturnLocation()) bool(strvex(value).to_bool());
        }
        else if (type.IsInt8) {
            new (gen->GetAddressOfReturnLocation()) int8(numeric_cast<int8>(strvex(value).to_int64()));
        }
        else if (type.IsInt16) {
            new (gen->GetAddressOfReturnLocation()) int16(numeric_cast<int16>(strvex(value).to_int64()));
        }
        else if (type.IsInt32) {
            new (gen->GetAddressOfReturnLocation()) int32(numeric_cast<int32>(strvex(value).to_int64()));
        }
        else if (type.IsInt64) {
            new (gen->GetAddressOfReturnLocation()) int64(numeric_cast<int64>(strvex(value).to_int64()));
        }
        else if (type.IsUInt8) {
            new (gen->GetAddressOfReturnLocation()) uint8(numeric_cast<uint8>(strvex(value).to_int64()));
        }
        else if (type.IsUInt16) {
            new (gen->GetAddressOfReturnLocation()) uint16(numeric_cast<uint16>(strvex(value).to_int64()));
        }
        else if (type.IsUInt32) {
            new (gen->GetAddressOfReturnLocation()) uint32(numeric_cast<uint32>(strvex(value).to_int64()));
        }
        else if (type.IsUInt64) {
            new (gen->GetAddressOfReturnLocation()) uint64(numeric_cast<uint64>(strvex(value).to_int64()));
        }
        else if (type.IsSingleFloat) {
            new (gen->GetAddressOfReturnLocation()) float32(strvex(value).to_float32());
        }
        else if (type.IsDoubleFloat) {
            new (gen->GetAddressOfReturnLocation()) float64(strvex(value).to_float64());
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
        value = strex("{}", *cast_from_void<const int32*>(new_value));
    }
    else if (type.IsPrimitive) {
        if (type.IsBool) {
            value = strex("{}", *cast_from_void<const bool*>(new_value));
        }
        else if (type.IsInt8) {
            value = strex("{}", *cast_from_void<const int8*>(new_value));
        }
        else if (type.IsInt16) {
            value = strex("{}", *cast_from_void<const int16*>(new_value));
        }
        else if (type.IsInt32) {
            value = strex("{}", *cast_from_void<const int32*>(new_value));
        }
        else if (type.IsInt64) {
            value = strex("{}", *cast_from_void<const int64*>(new_value));
        }
        else if (type.IsUInt8) {
            value = strex("{}", *cast_from_void<const uint8*>(new_value));
        }
        else if (type.IsUInt16) {
            value = strex("{}", *cast_from_void<const uint16*>(new_value));
        }
        else if (type.IsUInt32) {
            value = strex("{}", *cast_from_void<const uint32*>(new_value));
        }
        else if (type.IsUInt64) {
            value = strex("{}", *cast_from_void<const uint64*>(new_value));
        }
        else if (type.IsSingleFloat) {
            value = strex("{}", *cast_from_void<const float32*>(new_value));
        }
        else if (type.IsDoubleFloat) {
            value = strex("{}", *cast_from_void<const float64*>(new_value));
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

void RegisterAngelScriptEnums(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    const auto* meta = GetEngineMetadata(as_engine);

    for (auto&& [enum_name, enum_pairs] : meta->GetAllEnums()) {
        FO_AS_VERIFY(as_engine->RegisterEnum(enum_name.c_str()));

        for (auto&& [key, value] : enum_pairs) {
            FO_AS_VERIFY(as_engine->RegisterEnumValue(enum_name.c_str(), key.c_str(), value));
        }
    }
}

void RegisterAngelScriptGlobals(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    auto* backend = GetScriptBackend(as_engine);
    const auto* meta = backend->GetMetadata();

    // Global functions
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition)", FO_SCRIPT_GENERIC(Global_Assert<0>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1)", FO_SCRIPT_GENERIC(Global_Assert<1>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2)", FO_SCRIPT_GENERIC(Global_Assert<2>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3)", FO_SCRIPT_GENERIC(Global_Assert<3>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", FO_SCRIPT_GENERIC(Global_Assert<4>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", FO_SCRIPT_GENERIC(Global_Assert<5>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", FO_SCRIPT_GENERIC(Global_Assert<6>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", FO_SCRIPT_GENERIC(Global_Assert<7>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", FO_SCRIPT_GENERIC(Global_Assert<8>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", FO_SCRIPT_GENERIC(Global_Assert<9>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", FO_SCRIPT_GENERIC(Global_Assert<10>), FO_SCRIPT_GENERIC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message)", FO_SCRIPT_GENERIC(Global_ThrowException<0>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1)", FO_SCRIPT_GENERIC(Global_ThrowException<1>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2)", FO_SCRIPT_GENERIC(Global_ThrowException<2>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3)", FO_SCRIPT_GENERIC(Global_ThrowException<3>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", FO_SCRIPT_GENERIC(Global_ThrowException<4>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", FO_SCRIPT_GENERIC(Global_ThrowException<5>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", FO_SCRIPT_GENERIC(Global_ThrowException<6>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", FO_SCRIPT_GENERIC(Global_ThrowException<7>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", FO_SCRIPT_GENERIC(Global_ThrowException<8>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", FO_SCRIPT_GENERIC(Global_ThrowException<9>), FO_SCRIPT_GENERIC_CONV));
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", FO_SCRIPT_GENERIC(Global_ThrowException<10>), FO_SCRIPT_GENERIC_CONV));

    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("void Yield(int durationMs)", FO_SCRIPT_FUNC(Global_Yield), FO_SCRIPT_FUNC_CONV));

    // Global instances
    FO_AS_VERIFY(as_engine->RegisterGlobalFunction("GameSingleton@ get_Game()", FO_SCRIPT_GENERIC(Global_GetGame), FO_SCRIPT_GENERIC_CONV));

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
            auto* enums_arr = CreateScriptArray(as_engine, strex("{}Property[]", registrator->GetTypeName()).c_str());
            backend->AddCleanupCallback([enums_arr]() FO_DEFERRED { enums_arr->Release(); });
            enums_arr->Reserve(numeric_cast<int32>(properties.size()));

            for (const auto& prop : properties) {
                const int32 e = prop->GetRegIndex();
                enums_arr->InsertLast(cast_to_void(&e));
            }

            FO_AS_VERIFY(as_engine->SetDefaultNamespace(strex("{}PropertyGroup", registrator->GetTypeName()).c_str()));
            FO_AS_VERIFY(as_engine->RegisterGlobalFunction(strex("{}Property[]@+ get_{}()", registrator->GetTypeName(), group_name).c_str(), FO_SCRIPT_GENERIC(Global_GetPropertyGroup), FO_SCRIPT_GENERIC_CONV, cast_to_void(enums_arr)));
            FO_AS_VERIFY(as_engine->SetDefaultNamespace(""));
        }
    }

    // Settings
    FO_AS_VERIFY(as_engine->RegisterObjectType("GlobalSettings", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
    FO_AS_VERIFY(as_engine->RegisterGlobalProperty("GlobalSettings Settings", cast_to_void(meta)));

    const auto register_engine_setting = [&]<typename T>(const char* name, T& data, bool writeble) {
        string type_str;

        if constexpr (std::is_same_v<T, int32>) {
            type_str = "int";
        }
        else if constexpr (std::is_same_v<T, int64>) {
            type_str = "int64";
        }
        else if constexpr (std::is_same_v<T, float32>) {
            type_str = "float";
        }
        else if constexpr (std::is_same_v<T, float64>) {
            type_str = "double";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            type_str = "bool";
        }
        else if constexpr (std::is_same_v<T, string>) {
            type_str = "string";
        }

        if (!type_str.empty()) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GlobalSettings", strex("{} get_{}()", type_str, name).c_str(), FO_SCRIPT_GENERIC(Setting_GetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&data)));

            if (writeble) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GlobalSettings", strex("void set_{}({} value)", name, type_str).c_str(), FO_SCRIPT_GENERIC(Setting_SetEngineValue<T>), FO_SCRIPT_GENERIC_CONV, cast_to_void(&data)));
            }
        }
    };

    static GlobalSettings dummy_settings(false);
    GlobalSettings& settings = backend->HasGameEngine() ? backend->GetGameEngine()->Settings : dummy_settings;

#define FIXED_SETTING(type, name, ...) register_engine_setting.operator()<type>(#name, const_cast<type&>(settings.name), false)
#define VARIABLE_SETTING(type, name, ...) register_engine_setting.operator()<type>(#name, settings.name, true)
#define SETTING_GROUP(name, ...)
#define SETTING_GROUP_END()
#include "Settings-Include.h"

    for (const auto& [setting_name, setting_type] : meta->GetGameSettings()) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GlobalSettings", strex("{} get_{}()", MakeScriptTypeName(*setting_type), setting_name).c_str(), FO_SCRIPT_GENERIC(Setting_GetValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(&setting_name)));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GlobalSettings", strex("void set_{}({} value)", setting_name, MakeScriptTypeName(*setting_type)).c_str(), FO_SCRIPT_GENERIC(Setting_SetValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(&setting_name)));
    }
}

FO_END_NAMESPACE

#endif
