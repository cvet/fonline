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
#include "AngelScriptAttributes.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptDict.h"
#include "AngelScriptHelpers.h"

#include <angelscript.h>
#include <preprocessor.h>

FO_BEGIN_NAMESPACE

static auto CollectModuleScriptFunctions(ptr<const AngelScript::asIScriptModule> mod) -> vector<ptr<const AngelScript::asIScriptFunction>>
{
    vector<ptr<const AngelScript::asIScriptFunction>> funcs;

    for (AngelScript::asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        nptr<const AngelScript::asIScriptFunction> func = mod->GetFunctionByIndex(i);

        if (func && (func->GetFuncType() == AngelScript::asFUNC_SCRIPT || func->GetFuncType() == AngelScript::asFUNC_VIRTUAL)) {
            funcs.emplace_back(func);
        }
    }

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        nptr<const AngelScript::asITypeInfo> object_type = mod->GetObjectTypeByIndex(i);

        if (!object_type) {
            continue;
        }

        for (AngelScript::asUINT j = 0; j < object_type->GetMethodCount(); j++) {
            nptr<const AngelScript::asIScriptFunction> func = object_type->GetMethodByIndex(j, false);

            if (func && (func->GetFuncType() == AngelScript::asFUNC_SCRIPT || func->GetFuncType() == AngelScript::asFUNC_VIRTUAL)) {
                funcs.emplace_back(func);
            }
        }
    }

    return funcs;
}

static auto GetFunctionDeclarationString(nptr<const AngelScript::asIScriptFunction> func) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!func) {
        return "<unknown>";
    }

    nptr<const char> declaration = func->GetDeclaration(true, true, false);
    return declaration ? declaration.get() : "<unknown>";
}

static auto ResolveDeclaredFunctionSourceLocation(nptr<const AngelScript::asIScriptFunction> func, nptr<const Preprocessor::LineNumberTranslator> lnt) -> optional<pair<string, uint32_t>>
{
    FO_STACK_TRACE_ENTRY();

    if (!func) {
        return std::nullopt;
    }

    int row = 0;
    int column = 1;
    nptr<const char> section;

    if (func->GetDeclaredAt(section.get_pp(), &row, &column) < 0 || row <= 0) {
        return std::nullopt;
    }

    if (lnt) {
        auto line = numeric_cast<uint32_t>(row);
        return pair {string {Preprocessor::ResolveOriginalFile(line, lnt.get())}, Preprocessor::ResolveOriginalLine(line, lnt.get())};
    }

    return pair {section ? string {section.get()} : string {}, numeric_cast<uint32_t>(row)};
}

static auto MakeRemoteCallImplementationDecl(const EngineMetadata& meta, const RemoteCallDesc& inbound_call) -> string
{
    string_view ns = strvex(inbound_call.SubsystemHint).erase_file_extension();

    if (meta.GetSide() == EngineSideKind::ServerSide) {
        string args = MakeScriptArgsName(inbound_call.Args);
        return strex("void {}::{}(Player@+ player{}{})", ns, inbound_call.Name, !args.empty() ? ", " : "", args);
    }

    return strex("void {}::{}({})", ns, inbound_call.Name, MakeScriptArgsName(inbound_call.Args));
}

static auto RemoteCallConstObjectBytes(nptr<const void> obj) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(obj, "Remote call object is null");
    return cast_from_void<const uint8_t*>(obj.get());
}

template<typename T>
static auto RemoteCallConstObjectAs(nptr<const void> obj) noexcept -> ptr<const T>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(obj, "Remote call object is null");
    return cast_from_void<const T*>(obj.get());
}

static auto GetConstStructFieldStorage(nptr<const void> obj, size_t offset) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto bytes = RemoteCallConstObjectBytes(obj);
    return bytes.offset(offset);
}

static auto ReadMutableObjectHandleSlot(nptr<const void> slot) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!slot) {
        return nullptr;
    }

    return *slot.reinterpret_as<void*>();
}

static auto ResolveInboundRemoteCallImplementation(ptr<const AngelScript::asIScriptModule> mod, const EngineMetadata& meta, const RemoteCallDesc& inbound_call) -> nptr<AngelScript::asIScriptFunction>
{
    string func_decl = MakeRemoteCallImplementationDecl(meta, inbound_call);
    return mod->GetFunctionByDecl(func_decl.c_str());
}

static void OutboundRemoteCallFunc(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptGeneric> generic = gen;
    ptr<AngelScript::asIScriptEngine> as_engine = generic->GetEngine();
    auto engine = GetGameEngine(as_engine);
    auto caller = GetGenericObjectAs<Entity>(generic);
    auto outbound_call = GetGenericAuxiliaryAs<const RemoteCallDesc>(generic);

    vector<uint8_t> data;
    DataWriter writer(data);

    function<void(nptr<const void>, const BaseTypeDesc&)> write_simple = [&](nptr<const void> value, const BaseTypeDesc& type) {
        if (type.IsPrimitive) {
            FO_VERIFY_AND_THROW(value, "Primitive argument value is null");
            VisitBaseTypePrimitive(value.get(), type, [&](auto&& v) {
                using t = std::decay_t<decltype(v)>;
                writer.Write<t>(v);
            });
        }
        else if (type.IsEnum) {
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum underlying type is null");
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
            auto enum_data_bytes = RemoteCallConstObjectBytes(value);
            writer.WriteBytes({enum_data_bytes.get(), type.Size});
        }
        else if (type.IsString) {
            auto str = RemoteCallConstObjectAs<string>(value);
            writer.Write<int32_t>(numeric_cast<int32_t>(str->length()));
            writer.WriteStringBytes(*str);
        }
        else if (type.IsHashedString) {
            auto hstr = RemoteCallConstObjectAs<hstring>(value);
            writer.Write<hstring::hash_t>(hstr->as_hash());
        }
        else if (type.IsRefType) {
            auto ref_obj = ReadMutableObjectHandleSlot(value);
            auto raw_data = ConvertRefTypeScriptObjectToRawData(type, ref_obj);
            writer.Write<uint32_t>(numeric_cast<uint32_t>(raw_data.size()));

            if (!raw_data.empty()) {
                writer.WriteBytes({raw_data.data(), raw_data.size()});
            }
        }
        else if (type.IsStruct) {
            for (const auto& field : type.StructLayout->Fields) {
                write_simple(GetConstStructFieldStorage(value, field.Offset), field.Type);
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    };

    for (size_t i = 0; i < outbound_call->Args.size(); i++) {
        auto arg = make_ptr(&outbound_call->Args[i]);
        auto arg_ptr = GetGenericAddressArg(generic, numeric_cast<AngelScript::asUINT>(i));

        if (arg->Type.Kind == ComplexTypeKind::Simple) {
            write_simple(arg_ptr, arg->Type.BaseType);
        }
        else if (arg->Type.Kind == ComplexTypeKind::Array) {
            auto arr = NativeDataProvider::ReadConstTypedHandleSlot<ScriptArray>(arg_ptr);
            int32_t arr_size = arr ? numeric_cast<int32_t>(arr->GetSize()) : 0;
            writer.Write<int32_t>(arr_size);

            if (arr) {
                for (int32_t j = 0; j < arr_size; j++) {
                    ptr<void> value = arr->At(j);
                    write_simple(value, arg->Type.BaseType);
                }
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::Dict) {
            auto dict = NativeDataProvider::ReadConstTypedHandleSlot<ScriptDict>(arg_ptr);
            int32_t dict_size = dict ? numeric_cast<int32_t>(dict->GetSize()) : 0;
            writer.Write<int32_t>(dict_size);

            if (dict) {
                for (const auto& kv : *dict->GetMap()) {
                    nptr<const void> key = kv.first;
                    nptr<const void> value = kv.second;
                    write_simple(key, arg->Type.KeyType.value());
                    write_simple(value, arg->Type.BaseType);
                }
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::DictOfArray) {
            auto dict = NativeDataProvider::ReadConstTypedHandleSlot<ScriptDict>(arg_ptr);
            int32_t dict_size = dict ? numeric_cast<int32_t>(dict->GetSize()) : 0;
            writer.Write<int32_t>(dict_size);

            if (dict) {
                for (const auto& kv : *dict->GetMap()) {
                    nptr<const void> key = kv.first;
                    write_simple(key, arg->Type.KeyType.value());

                    auto arr = NativeDataProvider::ReadConstTypedHandleSlot<ScriptArray>(kv.second);
                    int32_t arr_size = arr ? numeric_cast<int32_t>(arr->GetSize()) : 0;
                    writer.Write<int32_t>(arr_size);

                    if (arr) {
                        for (int32_t j = 0; j < arr_size; j++) {
                            ptr<void> value = arr->At(j);
                            write_simple(value, arg->Type.BaseType);
                        }
                    }
                }
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    }

    engine->SendRemoteCall(outbound_call->Name, caller, data);
}

static void InboundRemoteCallHandler(const RemoteCallDesc& inbound_call, nptr<Entity> entity, span<uint8_t> data, ptr<BaseEngine> engine, ptr<AngelScript::asIScriptFunction> func)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(engine->GetSide() != EngineSideKind::MapperSide, "Remote calls are not supported on mapper side");

    ptr<AngelScript::asIScriptEngine> as_engine = func->GetEngine();
    MutableDataReader reader(data);

    struct RemoteCallPlainArgData
    {
        alignas(uint64_t) uint8_t Bytes[sizeof(uint64_t)] {};
    };

    using possible_types = variant<RemoteCallPlainArgData, string, hstring, vector<uint8_t>, refcount_ptr<DynamicRefTypeInstance>, refcount_ptr<ScriptArray>, refcount_ptr<ScriptDict>>;
    list<possible_types> temp_data;

    auto read_plain_data = [&](size_t size) -> nptr<void> {
        FO_VERIFY_AND_THROW(size <= sizeof(uint64_t), "Remote call plain argument is too large", size, sizeof(uint64_t));
        RemoteCallPlainArgData& storage = std::get<RemoteCallPlainArgData>(temp_data.emplace_back(RemoteCallPlainArgData {}));
        ptr<uint8_t> storage_bytes = storage.Bytes;
        reader.ReadBytes({storage_bytes.get(), size});
        return storage_bytes.void_cast();
    };

    function<nptr<void>(const BaseTypeDesc&)> read_simple = [&](const BaseTypeDesc& type) -> nptr<void> {
        if (type.IsPrimitive) {
            return read_plain_data(type.Size);
        }
        else if (type.IsEnum) {
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum underlying type is null");
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
            return read_plain_data(type.Size);
        }
        else if (type.IsString) {
            int32_t str_len = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(str_len >= 0, "Str len is negative", str_len);
            size_t str_size = numeric_cast<size_t>(str_len);
            string str;
            str.resize(str_size);
            reader.ReadStringBytes(str);

            auto str_value = make_ptr(&std::get<string>(temp_data.emplace_back(std::move(str))));
            return str_value.void_cast();
        }
        else if (type.IsHashedString) {
            auto hash = reader.Read<hstring::hash_t>();
            hstring hstr = engine->Hashes.ResolveHash(hash);
            auto hstr_value = make_ptr(&std::get<hstring>(temp_data.emplace_back(hstring(hstr))));
            return hstr_value.void_cast();
        }
        else if (type.IsRefType) {
            uint32_t raw_size = reader.Read<uint32_t>();
            span<uint8_t> ref_raw_data = reader.ReadBytes(raw_size);

            auto ref_obj = CreateRefTypeScriptObjectFromRawData(type, ref_raw_data);
            ptr<refcount_ptr<DynamicRefTypeInstance>> ref_obj_ptr = &std::get<refcount_ptr<DynamicRefTypeInstance>>(temp_data.emplace_back(std::move(ref_obj)));
            auto ref_obj_handle = make_ptr(ref_obj_ptr->get_pp()).reinterpret_as<void>();
            return ref_obj_handle;
        }
        else if (type.IsStruct) {
            ptr<vector<uint8_t>> buf = &std::get<vector<uint8_t>>(temp_data.emplace_back(vector<uint8_t>(type.Size, 0)));
            for (const auto& field : type.StructLayout->Fields) {
                auto field_data = read_simple(field.Type);

                if (field.Type.Size != 0) {
                    FO_VERIFY_AND_THROW(field_data, "Decoded struct field data is null");
                    size_t field_pos = field.Offset;
                    span_write_bytes(make_span(*buf), field_pos, make_span(field_data.get(), field.Type.Size));
                }
            }
            nptr<uint8_t> buf_data = buf->data();
            if (!buf_data) {
                return nullptr;
            }
            return buf_data.void_cast();
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    };

    auto require_value_ptr = [](nptr<void> value) -> ptr<void> {
        FO_VERIFY_AND_THROW(value, "Decoded argument value is null");
        return value;
    };

    auto accessor = make_ptr(&SCRIPT_DATA_ACCESSOR);
    FuncCallData call {.Accessor = accessor};
    size_t args_count = inbound_call.Args.size() + (engine->GetSide() == EngineSideKind::ServerSide ? 1 : 0);
    vector<ptr<void>> data_storage;
    data_storage.reserve(args_count);

    size_t arg_index = 0;

    if (engine->GetSide() == EngineSideKind::ServerSide) {
        FO_VERIFY_AND_THROW(inbound_call.Args.size() + 1 == func->GetParamCount(), "Inbound server remote call argument count does not match function signature", inbound_call.Name, inbound_call.Args.size(), func->GetParamCount());
        // Store a pointer into the entity parameter's own handle slot: the parameter outlives the ScriptFuncCall
        // below. A local copy would be destroyed at the end of this block, leaving data_storage dangling.
        data_storage.emplace_back(make_ptr(entity.get_pp()).void_cast());
        arg_index++;
    }
    else {
        FO_VERIFY_AND_THROW(inbound_call.Args.size() == func->GetParamCount(), "Inbound remote call argument count does not match function signature", inbound_call.Name, inbound_call.Args.size(), func->GetParamCount());
    }

    for (size_t i = 0; i < inbound_call.Args.size(); i++) {
        auto arg = make_ptr(&inbound_call.Args[i]);

        if (arg->Type.Kind == ComplexTypeKind::Simple) {
            nptr<void> value = read_simple(arg->Type.BaseType);
            data_storage.emplace_back(require_value_ptr(value));
            arg_index++;
        }
        else if (arg->Type.Kind == ComplexTypeKind::Array) {
            int32_t arr_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(arr_size >= 0, "Arr size is negative", arr_size);
            auto arr_holder = CreateScriptArray(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto arr = arr_holder.as_ptr();
            ptr<refcount_ptr<ScriptArray>> arr_ref = &std::get<refcount_ptr<ScriptArray>>(temp_data.emplace_back(std::move(arr_holder)));
            data_storage.emplace_back(make_ptr(arr_ref->get_pp()).void_cast());
            arg_index++;
            arr->Reserve(arr_size);

            for (int32_t j = 0; j < arr_size; j++) {
                nptr<void> value = read_simple(arg->Type.BaseType);
                auto value_ptr = require_value_ptr(value);
                arr->InsertLast(value_ptr);
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::Dict) {
            int32_t dict_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(dict_size >= 0, "Dict size is negative", dict_size);
            auto dict_holder = CreateScriptDict(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto dict = dict_holder.as_ptr();
            ptr<refcount_ptr<ScriptDict>> dict_ref = &std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(std::move(dict_holder)));
            data_storage.emplace_back(make_ptr(dict_ref->get_pp()).void_cast());
            arg_index++;

            for (int32_t j = 0; j < dict_size; j++) {
                nptr<void> key = read_simple(arg->Type.KeyType.value());
                nptr<void> value = read_simple(arg->Type.BaseType);
                dict->Set(require_value_ptr(key), require_value_ptr(value));
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::DictOfArray) {
            int32_t dict_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(dict_size >= 0, "Dict size is negative", dict_size);
            auto dict_holder = CreateScriptDict(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto dict = dict_holder.as_ptr();
            ptr<refcount_ptr<ScriptDict>> dict_ref = &std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(std::move(dict_holder)));
            data_storage.emplace_back(make_ptr(dict_ref->get_pp()).void_cast());
            arg_index++;

            for (int32_t j = 0; j < dict_size; j++) {
                nptr<void> key = read_simple(arg->Type.KeyType.value());

                int32_t arr_size = reader.Read<int32_t>();
                FO_VERIFY_AND_THROW(arr_size >= 0, "Arr size is negative", arr_size);
                auto arr = CreateScriptArray(as_engine, strex("array<{}>", MakeScriptTypeName(arg->Type.BaseType)).c_str());

                for (int32_t l = 0; l < arr_size; l++) {
                    nptr<void> value = read_simple(arg->Type.BaseType);
                    auto value_ptr = require_value_ptr(value);
                    arr->InsertLast(value_ptr);
                }

                dict->Set(require_value_ptr(key), require_value_ptr(make_nptr(arr.get()).void_cast()));
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    }

    reader.VerifyEnd();
    FO_VERIFY_AND_THROW(arg_index == args_count, "Decoded argument count does not match expected count");
    call.ArgsData = const_span<ptr<void>> {data_storage.data(), data_storage.size()};

    try {
        ScriptFuncCall(func, call);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

void RegisterAngelScriptRemoteCalls(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto meta = GetEngineMetadata(as_engine);

    FO_AS_VERIFY(as_engine->RegisterObjectType("RemoteCaller", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));

    if (meta->GetSide() == EngineSideKind::ServerSide) {
        FO_AS_VERIFY(as_engine->RegisterObjectType("CritterRemoteCaller", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
    }

    for (const auto& outbound_call : (*meta->GetOutboundRemoteCalls()) | std::views::values) {
        string method_decl = strex("void {}({})", outbound_call.Name, MakeScriptArgsName(outbound_call.Args));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("RemoteCaller", method_decl.c_str(), FO_SCRIPT_GENERIC(OutboundRemoteCallFunc), FO_SCRIPT_GENERIC_CONV, make_nptr(&outbound_call).void_cast()));

        if (meta->GetSide() == EngineSideKind::ServerSide) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("CritterRemoteCaller", method_decl.c_str(), FO_SCRIPT_GENERIC(OutboundRemoteCallFunc), FO_SCRIPT_GENERIC_CONV, make_nptr(&outbound_call).void_cast()));
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

void BindAngelScriptRemoteCalls(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const AngelScript::asIScriptModule> as_module = as_engine->GetModuleByIndex(0);
    FO_VERIFY_AND_THROW(as_module, "Missing required AngelScript module");
    auto backend = GetScriptBackend(as_engine);
    auto meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Missing engine metadata");

    for (const auto& inbound_call : (*meta->GetInboundRemoteCalls()) | std::views::values) {
        if (!strvex(inbound_call.SubsystemHint).ends_with("fos")) {
            continue;
        }

        if (auto func = ResolveInboundRemoteCallImplementation(as_module, *meta, inbound_call)) {
            if (backend->HasGameEngine()) {
                auto engine = backend->GetGameEngine();
                engine->SetRemoteCallHandler(inbound_call.Name, [&inbound_call, engine, func = func.as_ptr()](hstring name, nptr<Entity> entity, span<uint8_t> data) FO_DEFERRED {
                    FO_VERIFY_AND_THROW(name == inbound_call.Name, "Inbound remote call name changed while dispatching");
                    InboundRemoteCallHandler(inbound_call, entity, data, engine, func);
                });
            }
        }
        else {
            throw ScriptCallException("Remote call function not found", MakeRemoteCallImplementationDecl(*meta, inbound_call));
        }
    }
}

auto ValidateAngelScriptRemoteCallAttributes(ptr<const AngelScript::asIScriptModule> mod, const EngineMetadata& meta, nptr<const Preprocessor::LineNumberTranslator> lnt) -> string
{
    string errors;
    string_view expected_attr {};
    string_view opposite_attr {};

    switch (meta.GetSide()) {
    case EngineSideKind::ServerSide:
        expected_attr = "ServerRemoteCall";
        opposite_attr = "ClientRemoteCall";
        break;
    case EngineSideKind::ClientSide:
        expected_attr = "ClientRemoteCall";
        opposite_attr = "ServerRemoteCall";
        break;
    case EngineSideKind::MapperSide:
        break;
    default:
        FO_UNREACHABLE_PLACE();
    }

    if (expected_attr.empty()) {
        return errors;
    }

    vector<ptr<const AngelScript::asIScriptFunction>> matched_funcs;

    for (const auto& inbound_call : (*meta.GetInboundRemoteCalls()) | std::views::values) {
        if (!strvex(inbound_call.SubsystemHint).ends_with("fos")) {
            continue;
        }

        if (auto func = ResolveInboundRemoteCallImplementation(mod, meta, inbound_call)) {
            if (std::ranges::find(matched_funcs, func) == matched_funcs.end()) {
                matched_funcs.emplace_back(func);
            }
        }
    }

    auto append_error = [&errors](optional<pair<string, uint32_t>> location, const string& error) {
        if (!errors.empty()) {
            errors.append("\n");
        }

        if (location.has_value()) {
            errors.append(strex("{}({},1): error : {}", location->first, location->second, error).str());
        }
        else {
            errors.append(error);
        }
    };

    for (ptr<const AngelScript::asIScriptFunction> func : matched_funcs) {
        if (!HasFunctionAttribute(func.get(), expected_attr)) {
            string func_decl = GetFunctionDeclarationString(func);
            string message = strex("Inbound ///@ RemoteCall implementation '{}' must be marked [[{}]]", func_decl, expected_attr).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
    }

    for (ptr<const AngelScript::asIScriptFunction> func : CollectModuleScriptFunctions(mod)) {
        if (HasFunctionAttribute(func.get(), opposite_attr)) {
            string func_decl = GetFunctionDeclarationString(func);
            string message = strex("Functions marked [[{}]] must correspond to inbound ///@ RemoteCall declarations, '{}' uses the wrong remote-call attribute for this engine side", opposite_attr, func_decl).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
        else if (HasFunctionAttribute(func.get(), expected_attr) && std::ranges::find(matched_funcs, func) == matched_funcs.end()) {
            string func_decl = GetFunctionDeclarationString(func);
            string message = strex("Functions marked [[{}]] must correspond to inbound ///@ RemoteCall declarations, '{}' has no matching ///@ RemoteCall declaration", expected_attr, func_decl).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
    }

    return errors;
}

FO_END_NAMESPACE

#endif
