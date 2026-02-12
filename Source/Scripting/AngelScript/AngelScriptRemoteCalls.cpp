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

#include "AngelScriptRemoteCalls.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"

FO_BEGIN_NAMESPACE

static void OutboundRemoteCallFunc(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* engine = GetGameEngine(gen->GetEngine());
    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto& outbound_call = *cast_from_void<const RemoteCallDesc*>(gen->GetAuxiliary());

    vector<uint8> data;
    DataWriter writer(data);

    const auto write_simple = [&](const void* ptr, const BaseTypeDesc& type) {
        if (type.IsPrimitive) {
            VisitBaseTypePrimitive(ptr, type, [&](auto&& v) {
                using t = std::decay_t<decltype(v)>;
                writer.Write<t>(v);
            });
        }
        else if (type.IsEnum) {
            const int32 enum_value = *cast_from_void<const int32*>(ptr);
            writer.Write<int32>(enum_value);
        }
        else if (type.IsString) {
            const auto& str = *cast_from_void<const string*>(ptr);
            writer.Write<int32>(numeric_cast<int32>(str.length()));
            writer.WritePtr(str.c_str(), str.length());
        }
        else if (type.IsHashedString) {
            const auto& hstr = *cast_from_void<const hstring*>(ptr);
            writer.Write<hstring::hash_t>(hstr.as_hash());
        }
        else if (type.IsStruct) {
            writer.WritePtr(ptr, type.Size);
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    };

    for (size_t i = 0; i < outbound_call.Args.size(); i++) {
        const auto& arg = outbound_call.Args[i];
        void* arg_ptr = gen->GetAddressOfArg(numeric_cast<AngelScript::asUINT>(i));

        if (arg.Type.Kind == ComplexTypeKind::Simple) {
            write_simple(arg_ptr, arg.Type.BaseType);
        }
        else if (arg.Type.Kind == ComplexTypeKind::Array) {
            const auto* arr = *cast_from_void<const ScriptArray**>(arg_ptr);
            const auto arr_size = numeric_cast<int32>(arr->GetSize());
            writer.Write<int32>(arr_size);

            for (int32 j = 0; j < arr_size; j++) {
                write_simple(arr->At(j), arg.Type.BaseType);
            }
        }
        else if (arg.Type.Kind == ComplexTypeKind::Dict) {
            const auto* dict = *cast_from_void<const ScriptDict**>(arg_ptr);
            const auto dict_size = numeric_cast<int32>(dict->GetSize());
            writer.Write<int32>(dict_size);

            for (const auto& kv : dict->GetMap()) {
                write_simple(kv.first, arg.Type.KeyType.value());
                write_simple(kv.second, arg.Type.BaseType);
            }
        }
        else if (arg.Type.Kind == ComplexTypeKind::DictOfArray) {
            const auto* dict = *cast_from_void<const ScriptDict**>(arg_ptr);
            const auto dict_size = numeric_cast<int32>(dict->GetSize());
            writer.Write<int32>(dict_size);

            for (const auto& kv : dict->GetMap()) {
                write_simple(kv.first, arg.Type.KeyType.value());

                const auto* arr = *cast_from_void<const ScriptArray**>(kv.second);
                const auto arr_size = numeric_cast<int32>(arr->GetSize());
                writer.Write<int32>(arr_size);

                for (int32 j = 0; j < arr_size; j++) {
                    write_simple(arr->At(j), arg.Type.BaseType);
                }
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    }

    engine->SendRemoteCall(outbound_call.Name, entity, data);
}

static void InboundRemoteCallHandler(const RemoteCallDesc& inbound_call, Entity* entity, span<uint8> data, BaseEngine* engine, AngelScript::asIScriptFunction* func)
{
    FO_STACK_TRACE_ENTRY();

    // Todo: add content verification for server inbound data
    FO_RUNTIME_ASSERT(engine->GetSide() != EngineSideKind::MapperSide);

    auto* as_engine = func->GetEngine();
    MutableDataReader reader(data);

    using possible_types = variant<int32, string, hstring, refcount_ptr<ScriptArray>, refcount_ptr<ScriptDict>>;
    list<possible_types> temp_data;

    const auto read_simple = [&](const BaseTypeDesc& type) -> void* {
        if (type.IsPrimitive) {
            return reader.ReadPtr<void>(type.Size);
        }
        else if (type.IsEnum) {
            return reader.ReadPtr<void>(sizeof(int32));
        }
        else if (type.IsString) {
            const auto str_len = reader.Read<int32>();
            FO_RUNTIME_ASSERT(str_len >= 0);
            const auto* s = reader.ReadPtr<char>(str_len);
            return cast_to_void(&std::get<string>(temp_data.emplace_back(string(s, str_len))));
        }
        else if (type.IsHashedString) {
            const auto hash = reader.Read<hstring::hash_t>();
            const auto hstr = engine->Hashes.ResolveHash(hash);
            return cast_to_void(&std::get<hstring>(temp_data.emplace_back(hstring(hstr))));
        }
        else if (type.IsStruct) {
            return reader.ReadPtr<void>(type.Size);
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    };

    FuncCallData call {.Accessor = &SCRIPT_DATA_ACCESSOR};
    const size_t args_count = inbound_call.Args.size() + (engine->GetSide() == EngineSideKind::ServerSide ? 1 : 0);
    array<void*, MAX_CALL_ARGS> data_storage;
    call.ArgsData = span(data_storage).subspan(0, args_count);

    size_t arg_index = 0;

    if (engine->GetSide() == EngineSideKind::ServerSide) {
        FO_RUNTIME_ASSERT(inbound_call.Args.size() + 1 == func->GetParamCount());
        data_storage[arg_index++] = cast_to_void(&entity);
    }
    else {
        FO_RUNTIME_ASSERT(inbound_call.Args.size() == func->GetParamCount());
    }

    for (const auto& arg : inbound_call.Args) {
        if (arg.Type.Kind == ComplexTypeKind::Simple) {
            void* value = read_simple(arg.Type.BaseType);
            data_storage[arg_index++] = value;
        }
        else if (arg.Type.Kind == ComplexTypeKind::Array) {
            const auto arr_size = reader.Read<int32>();
            FO_RUNTIME_ASSERT(arr_size >= 0);
            auto* arr = CreateScriptArray(as_engine, MakeScriptTypeName(arg.Type).c_str());
            data_storage[arg_index++] = cast_to_void(std::get<refcount_ptr<ScriptArray>>(temp_data.emplace_back(refcount_ptr(refcount_ptr<ScriptArray>::adopt, arr))).get_pp());
            arr->Reserve(arr_size);

            for (int32 j = 0; j < arr_size; j++) {
                void* value = read_simple(arg.Type.BaseType);
                arr->InsertLast(value);
            }
        }
        else if (arg.Type.Kind == ComplexTypeKind::Dict) {
            const auto dict_size = reader.Read<int32>();
            FO_RUNTIME_ASSERT(dict_size >= 0);
            auto* dict = CreateScriptDict(as_engine, MakeScriptTypeName(arg.Type).c_str());
            data_storage[arg_index++] = cast_to_void(std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(refcount_ptr(refcount_ptr<ScriptDict>::adopt, dict))).get_pp());

            for (int32 j = 0; j < dict_size; j++) {
                void* key = read_simple(arg.Type.KeyType.value());
                void* value = read_simple(arg.Type.BaseType);
                dict->Set(key, value);
            }
        }
        else if (arg.Type.Kind == ComplexTypeKind::DictOfArray) {
            const auto dict_size = reader.Read<int32>();
            FO_RUNTIME_ASSERT(dict_size >= 0);
            auto* dict = CreateScriptDict(as_engine, MakeScriptTypeName(arg.Type).c_str());
            data_storage[arg_index++] = cast_to_void(std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(refcount_ptr(refcount_ptr<ScriptDict>::adopt, dict))).get_pp());

            for (int32 j = 0; j < dict_size; j++) {
                void* key = read_simple(arg.Type.KeyType.value());

                const auto arr_size = reader.Read<int32>();
                FO_RUNTIME_ASSERT(arr_size >= 0);
                auto* arr = CreateScriptArray(as_engine, strex("{}[]", MakeScriptTypeName(arg.Type.BaseType)).c_str());

                for (int32 l = 0; l < arr_size; l++) {
                    void* value = read_simple(arg.Type.BaseType);
                    arr->InsertLast(value);
                }

                dict->Set(key, cast_to_void(arr));
                arr->Release();
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    }

    reader.VerifyEnd();

    try {
        ScriptFuncCall(func, call);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

void RegisterAngelScriptRemoteCalls(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    const auto* meta = GetEngineMetadata(as_engine);

    FO_AS_VERIFY(as_engine->RegisterObjectType("RemoteCaller", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));

    if (meta->GetSide() == EngineSideKind::ServerSide) {
        FO_AS_VERIFY(as_engine->RegisterObjectType("CritterRemoteCaller", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
    }

    for (const auto& outbound_call : meta->GetOutboundRemoteCalls() | std::views::values) {
        const string method_decl = strex("void {}({})", outbound_call.Name, MakeScriptArgsName(outbound_call.Args));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("RemoteCaller", method_decl.c_str(), FO_SCRIPT_GENERIC(OutboundRemoteCallFunc), FO_SCRIPT_GENERIC_CONV, cast_to_void(&outbound_call)));

        if (meta->GetSide() == EngineSideKind::ServerSide) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("CritterRemoteCaller", method_decl.c_str(), FO_SCRIPT_GENERIC(OutboundRemoteCallFunc), FO_SCRIPT_GENERIC_CONV, cast_to_void(&outbound_call)));
        }
    }

    if (meta->GetSide() == EngineSideKind::ServerSide) {
        FO_AS_VERIFY(as_engine->RegisterObjectProperty("Player", "RemoteCaller ClientCall", 0));
        FO_AS_VERIFY(as_engine->RegisterObjectProperty("Critter", "CritterRemoteCaller PlayerClientCall", 0));
    }

    if (meta->GetSide() == EngineSideKind::ClientSide) {
        FO_AS_VERIFY(as_engine->RegisterObjectProperty("Player", "RemoteCaller ServerCall", 0));
    }
}

void BindAngelScriptRemoteCalls(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    const auto* as_module = as_engine->GetModuleByIndex(0);
    FO_RUNTIME_ASSERT(as_module);
    auto* backend = GetScriptBackend(as_engine);
    const auto* meta = backend->GetMetadata();

    for (const auto& inbound_call : meta->GetInboundRemoteCalls() | std::views::values) {
        if (!strvex(inbound_call.SubsystemHint).ends_with("fos")) {
            continue;
        }

        const string ns = strex(inbound_call.SubsystemHint).erase_file_extension();
        string func_decl;

        if (meta->GetSide() == EngineSideKind::ServerSide) {
            const auto args = MakeScriptArgsName(inbound_call.Args);
            func_decl = strex("void {}::{}(Player@+ player{}{})", ns, inbound_call.Name, !args.empty() ? ", " : "", args);
        }
        else {
            func_decl = strex("void {}::{}({})", ns, inbound_call.Name, MakeScriptArgsName(inbound_call.Args));
        }

        if (auto* func = as_module->GetFunctionByDecl(func_decl.c_str()); func != nullptr) {
            if (backend->HasGameEngine()) {
                auto* engine = backend->GetGameEngine();
                engine->SetRemoteCallHandler(inbound_call.Name, [&inbound_call, engine, func](hstring name, Entity* entity, span<uint8> data) FO_DEFERRED {
                    FO_RUNTIME_ASSERT(name == inbound_call.Name);
                    InboundRemoteCallHandler(inbound_call, entity, data, engine, func);
                });
            }
        }
        else {
            throw ScriptCoreException("Remote call function not found", func_decl);
        }
    }
}

FO_END_NAMESPACE

#endif
