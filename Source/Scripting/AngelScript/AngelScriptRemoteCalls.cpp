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
        nptr<const AngelScript::asIScriptFunction> nullable_func = mod->GetFunctionByIndex(i);

        if (nullable_func && (nullable_func->GetFuncType() == AngelScript::asFUNC_SCRIPT || nullable_func->GetFuncType() == AngelScript::asFUNC_VIRTUAL)) {
            funcs.emplace_back(nullable_func.as_ptr());
        }
    }

    for (AngelScript::asUINT i = 0; i < mod->GetObjectTypeCount(); i++) {
        nptr<const AngelScript::asITypeInfo> object_type = mod->GetObjectTypeByIndex(i);

        if (!object_type) {
            continue;
        }

        for (AngelScript::asUINT j = 0; j < object_type->GetMethodCount(); j++) {
            nptr<const AngelScript::asIScriptFunction> nullable_func = object_type->GetMethodByIndex(j, false);

            if (nullable_func && (nullable_func->GetFuncType() == AngelScript::asFUNC_SCRIPT || nullable_func->GetFuncType() == AngelScript::asFUNC_VIRTUAL)) {
                funcs.emplace_back(nullable_func.as_ptr());
            }
        }
    }

    return funcs;
}

static auto RemoteCallBufferAt(span<uint8_t> data, size_t offset, size_t size) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(size != 0, "Buffer slice size is zero");
    FO_STRONG_ASSERT(offset < data.size(), "Buffer slice offset is out of range");
    FO_STRONG_ASSERT(size <= data.size() - offset, "Buffer slice extends past buffer end");

    return &data[offset];
}

static auto GetFunctionDeclarationString(nptr<const AngelScript::asIScriptFunction> func) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (!func) {
        return "<unknown>";
    }

    auto func_ptr = func.as_ptr();
    const nptr<const char> declaration = func_ptr->GetDeclaration(true, true, false);
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
        const auto line = numeric_cast<uint32_t>(row);
        return pair {string {Preprocessor::ResolveOriginalFile(line, lnt.get())}, Preprocessor::ResolveOriginalLine(line, lnt.get())};
    }

    return pair {section ? string {section.get()} : string {}, numeric_cast<uint32_t>(row)};
}

static auto MakeRemoteCallImplementationDecl(const EngineMetadata& meta, const RemoteCallDesc& inbound_call) -> string
{
    const string_view ns = strvex(inbound_call.SubsystemHint).erase_file_extension();

    if (meta.GetSide() == EngineSideKind::ServerSide) {
        const auto args = MakeScriptArgsName(inbound_call.Args);
        return strex("void {}::{}(Player@+ player{}{})", ns, inbound_call.Name, !args.empty() ? ", " : "", args);
    }

    return strex("void {}::{}({})", ns, inbound_call.Name, MakeScriptArgsName(inbound_call.Args));
}

template<typename T>
static auto GetGenericObjectAs(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<T> nullable_object = cast_from_void<T*>(gen->GetObject());
    FO_STRONG_ASSERT(nullable_object, "Generic call object is null");
    return nullable_object.as_ptr();
}

template<typename T>
static auto GetGenericAuxiliaryAs(ptr<AngelScript::asIScriptGeneric> gen) noexcept -> ptr<T>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<T> nullable_auxiliary = cast_from_void<T*>(gen->GetAuxiliary());
    FO_STRONG_ASSERT(nullable_auxiliary, "Generic call auxiliary is null");
    return nullable_auxiliary.as_ptr();
}

static auto GetGenericAddressArg(ptr<AngelScript::asIScriptGeneric> gen, AngelScript::asUINT arg_index) noexcept -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    nptr<void> nullable_arg_address = gen->GetAddressOfArg(arg_index);
    FO_STRONG_ASSERT(nullable_arg_address, "Generic call argument address is null");
    return nullable_arg_address.as_ptr();
}

static auto RemoteCallConstObjectBytes(nptr<const void> obj) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(obj, "Remote call object is null");
    auto obj_ptr = obj.as_ptr();
    return cast_from_void<const uint8_t*>(obj_ptr.get());
}

template<typename T>
static auto RemoteCallConstObjectAs(nptr<const void> obj) noexcept -> ptr<const T>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_STRONG_ASSERT(obj, "Remote call object is null");
    auto obj_ptr = obj.as_ptr();
    return cast_from_void<const T*>(obj_ptr.get());
}

static auto GetConstStructFieldStorage(nptr<const void> obj, size_t offset) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    auto bytes = RemoteCallConstObjectBytes(obj);
    return bytes.get() + offset;
}

static auto ReadMutableObjectHandleSlot(nptr<const void> slot) noexcept -> nptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!slot) {
        return nullptr;
    }

    return *static_cast<void* const*>(const_cast<void*>(slot.get_no_const()));
}

static auto ResolveInboundRemoteCallImplementation(ptr<const AngelScript::asIScriptModule> mod, const EngineMetadata& meta, const RemoteCallDesc& inbound_call) -> nptr<AngelScript::asIScriptFunction>
{
    const auto func_decl = MakeRemoteCallImplementationDecl(meta, inbound_call);
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

    const function<void(nptr<const void>, const BaseTypeDesc&)> write_simple = [&](nptr<const void> nullable_value, const BaseTypeDesc& type) {
        if (type.IsPrimitive) {
            FO_VERIFY_AND_THROW(nullable_value, "Primitive argument value is null");
            auto value = nullable_value.as_ptr();
            VisitBaseTypePrimitive(value.get(), type, [&](auto&& v) {
                using t = std::decay_t<decltype(v)>;
                writer.Write<t>(v);
            });
        }
        else if (type.IsEnum) {
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum underlying type is null");
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
            auto enum_data_bytes = RemoteCallConstObjectBytes(nullable_value);
            writer.WriteBytes({enum_data_bytes.get(), type.Size});
        }
        else if (type.IsString) {
            auto str = RemoteCallConstObjectAs<string>(nullable_value);
            writer.Write<int32_t>(numeric_cast<int32_t>(str->length()));
            writer.WriteStringBytes(*str);
        }
        else if (type.IsHashedString) {
            auto hstr = RemoteCallConstObjectAs<hstring>(nullable_value);
            writer.Write<hstring::hash_t>(hstr->as_hash());
        }
        else if (type.IsRefType) {
            const auto ref_obj = ReadMutableObjectHandleSlot(nullable_value);
            const auto raw_data = ConvertRefTypeScriptObjectToRawData(type, ref_obj);
            writer.Write<uint32_t>(numeric_cast<uint32_t>(raw_data.size()));

            if (!raw_data.empty()) {
                ptr<const uint8_t> raw_data_ptr = raw_data.data();
                writer.WriteBytes({raw_data_ptr.get(), raw_data.size()});
            }
        }
        else if (type.IsStruct) {
            for (const auto& field : type.StructLayout->Fields) {
                write_simple(GetConstStructFieldStorage(nullable_value, field.Offset), field.Type);
            }
        }
        else {
            throw NotSupportedException(FO_LINE_STR);
        }
    };

    for (size_t i = 0; i < outbound_call->Args.size(); i++) {
        ptr<const ArgDesc> arg = &outbound_call->Args[i];
        auto arg_ptr = GetGenericAddressArg(generic, numeric_cast<AngelScript::asUINT>(i));

        if (arg->Type.Kind == ComplexTypeKind::Simple) {
            write_simple(arg_ptr, arg->Type.BaseType);
        }
        else if (arg->Type.Kind == ComplexTypeKind::Array) {
            auto arr = NativeDataProvider::ReadConstTypedHandleSlot<ScriptArray>(arg_ptr);
            const auto arr_size = arr ? numeric_cast<int32_t>(arr->GetSize()) : 0;
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
            const auto dict_size = dict ? numeric_cast<int32_t>(dict->GetSize()) : 0;
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
            const auto dict_size = dict ? numeric_cast<int32_t>(dict->GetSize()) : 0;
            writer.Write<int32_t>(dict_size);

            if (dict) {
                for (const auto& kv : *dict->GetMap()) {
                    nptr<const void> key = kv.first;
                    write_simple(key, arg->Type.KeyType.value());

                    auto arr = NativeDataProvider::ReadConstTypedHandleSlot<ScriptArray>(kv.second);
                    const auto arr_size = arr ? numeric_cast<int32_t>(arr->GetSize()) : 0;
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

    using possible_types = variant<int32_t, string, hstring, vector<uint8_t>, refcount_ptr<DynamicRefTypeInstance>, refcount_ptr<ScriptArray>, refcount_ptr<ScriptDict>>;
    list<possible_types> temp_data;

    const function<nptr<void>(const BaseTypeDesc&)> read_simple = [&](const BaseTypeDesc& type) -> nptr<void> {
        if (type.IsPrimitive) {
            span<uint8_t> data = reader.ReadBytes(type.Size);
            if (data.empty()) {
                return nullptr;
            }

            ptr<uint8_t> value_data = data.data();
            ptr<void> value = cast_to_void(value_data.get());
            return value;
        }
        else if (type.IsEnum) {
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType, "Enum underlying type is null");
            FO_VERIFY_AND_THROW(type.EnumUnderlyingType->IsInt, "Enum underlying type is not integer");
            span<uint8_t> data = reader.ReadBytes(type.Size);
            if (data.empty()) {
                return nullptr;
            }

            ptr<uint8_t> value_data = data.data();
            ptr<void> value = cast_to_void(value_data.get());
            return value;
        }
        else if (type.IsString) {
            const auto str_len = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(str_len >= 0, "Str len is negative", str_len);
            const size_t str_size = numeric_cast<size_t>(str_len);
            string str;
            str.resize(str_size);
            reader.ReadStringBytes(str);

            ptr<string> str_value = &std::get<string>(temp_data.emplace_back(std::move(str)));
            ptr<void> str_storage = cast_to_void(str_value.get());
            return str_storage;
        }
        else if (type.IsHashedString) {
            const auto hash = reader.Read<hstring::hash_t>();
            const auto hstr = engine->Hashes.ResolveHash(hash);
            ptr<hstring> hstr_value = &std::get<hstring>(temp_data.emplace_back(hstring(hstr)));
            ptr<void> hstr_storage = cast_to_void(hstr_value.get());
            return hstr_storage;
        }
        else if (type.IsRefType) {
            const uint32_t raw_size = reader.Read<uint32_t>();
            span<uint8_t> ref_raw_data = reader.ReadBytes(raw_size);

            auto ref_obj = CreateRefTypeScriptObjectFromRawData(type, ref_raw_data);
            ptr<refcount_ptr<DynamicRefTypeInstance>> ref_obj_ptr = &std::get<refcount_ptr<DynamicRefTypeInstance>>(temp_data.emplace_back(std::move(ref_obj)));
            ptr<void> ref_obj_handle = static_cast<void*>(ref_obj_ptr->get_pp());
            return ref_obj_handle;
        }
        else if (type.IsStruct) {
            ptr<vector<uint8_t>> buf = &std::get<vector<uint8_t>>(temp_data.emplace_back(vector<uint8_t>(type.Size, 0)));
            for (const auto& field : type.StructLayout->Fields) {
                nptr<void> nullable_field_data = read_simple(field.Type);

                if (field.Type.Size != 0) {
                    FO_VERIFY_AND_THROW(nullable_field_data, "Decoded struct field data is null");
                    auto field_data = nullable_field_data.as_ptr();
                    span<uint8_t> buf_span {*buf};
                    auto field_dest = RemoteCallBufferAt(buf_span, field.Offset, field.Type.Size);
                    MemCopy(field_dest.get(), field_data.get(), field.Type.Size);
                }
            }
            nptr<uint8_t> buf_data = buf->data();
            if (!buf_data) {
                return nullptr;
            }
            ptr<void> struct_data = cast_to_void(buf_data.get());
            return struct_data;
        }
        else {
            FO_UNREACHABLE_PLACE();
        }
    };

    const auto require_value_ptr = [](nptr<void> nullable_value) -> ptr<void> {
        FO_VERIFY_AND_THROW(nullable_value, "Decoded argument value is null");
        return nullable_value.as_ptr();
    };

    ptr<const DataAccessor> accessor = &SCRIPT_DATA_ACCESSOR;
    FuncCallData call {.Accessor = accessor};
    const size_t args_count = inbound_call.Args.size() + (engine->GetSide() == EngineSideKind::ServerSide ? 1 : 0);
    vector<ptr<void>> data_storage;
    data_storage.reserve(args_count);

    size_t arg_index = 0;

    if (engine->GetSide() == EngineSideKind::ServerSide) {
        FO_VERIFY_AND_THROW(inbound_call.Args.size() + 1 == func->GetParamCount(), "Inbound server remote call argument count does not match function signature", inbound_call.Name, inbound_call.Args.size(), func->GetParamCount());
        nptr<Entity> entity_arg = entity;
        data_storage.emplace_back(static_cast<void*>(entity_arg.get_pp()));
        arg_index++;
    }
    else {
        FO_VERIFY_AND_THROW(inbound_call.Args.size() == func->GetParamCount(), "Inbound remote call argument count does not match function signature", inbound_call.Name, inbound_call.Args.size(), func->GetParamCount());
    }

    for (size_t i = 0; i < inbound_call.Args.size(); i++) {
        ptr<const ArgDesc> arg = &inbound_call.Args[i];

        if (arg->Type.Kind == ComplexTypeKind::Simple) {
            nptr<void> value = read_simple(arg->Type.BaseType);
            data_storage.emplace_back(require_value_ptr(value));
            arg_index++;
        }
        else if (arg->Type.Kind == ComplexTypeKind::Array) {
            const auto arr_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(arr_size >= 0, "Arr size is negative", arr_size);
            auto arr_holder = CreateScriptArray(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto arr = arr_holder.as_ptr();
            ptr<refcount_ptr<ScriptArray>> arr_ref = &std::get<refcount_ptr<ScriptArray>>(temp_data.emplace_back(std::move(arr_holder)));
            data_storage.emplace_back(static_cast<void*>(arr_ref->get_pp()));
            arg_index++;
            arr->Reserve(arr_size);

            for (int32_t j = 0; j < arr_size; j++) {
                nptr<void> value = read_simple(arg->Type.BaseType);
                ptr<void> value_ptr = require_value_ptr(value);
                arr->InsertLast(value_ptr);
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::Dict) {
            const auto dict_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(dict_size >= 0, "Dict size is negative", dict_size);
            auto dict_holder = CreateScriptDict(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto dict = dict_holder.as_ptr();
            ptr<refcount_ptr<ScriptDict>> dict_ref = &std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(std::move(dict_holder)));
            data_storage.emplace_back(static_cast<void*>(dict_ref->get_pp()));
            arg_index++;

            for (int32_t j = 0; j < dict_size; j++) {
                nptr<void> key = read_simple(arg->Type.KeyType.value());
                nptr<void> value = read_simple(arg->Type.BaseType);
                dict->Set(require_value_ptr(key), require_value_ptr(value));
            }
        }
        else if (arg->Type.Kind == ComplexTypeKind::DictOfArray) {
            const auto dict_size = reader.Read<int32_t>();
            FO_VERIFY_AND_THROW(dict_size >= 0, "Dict size is negative", dict_size);
            auto dict_holder = CreateScriptDict(as_engine, MakeScriptTypeName(arg->Type).c_str());
            auto dict = dict_holder.as_ptr();
            ptr<refcount_ptr<ScriptDict>> dict_ref = &std::get<refcount_ptr<ScriptDict>>(temp_data.emplace_back(std::move(dict_holder)));
            data_storage.emplace_back(static_cast<void*>(dict_ref->get_pp()));
            arg_index++;

            for (int32_t j = 0; j < dict_size; j++) {
                nptr<void> key = read_simple(arg->Type.KeyType.value());

                const auto arr_size = reader.Read<int32_t>();
                FO_VERIFY_AND_THROW(arr_size >= 0, "Arr size is negative", arr_size);
                auto arr_holder = CreateScriptArray(as_engine, strex("array<{}>", MakeScriptTypeName(arg->Type.BaseType)).c_str());
                auto arr = arr_holder.as_ptr();

                for (int32_t l = 0; l < arr_size; l++) {
                    nptr<void> value = read_simple(arg->Type.BaseType);
                    ptr<void> value_ptr = require_value_ptr(value);
                    arr->InsertLast(value_ptr);
                }

                dict->Set(require_value_ptr(key), require_value_ptr(cast_to_void(arr.get())));
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

void BindAngelScriptRemoteCalls(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    nptr<const AngelScript::asIScriptModule> nullable_as_module = as_engine->GetModuleByIndex(0);
    FO_VERIFY_AND_THROW(nullable_as_module, "Missing required AngelScript module");
    auto as_module = nullable_as_module.as_ptr();
    auto backend = GetScriptBackend(as_engine);
    nptr<const EngineMetadata> meta = backend->GetMetadata();

    for (const auto& inbound_call : (*meta->GetInboundRemoteCalls()) | std::views::values) {
        if (!strvex(inbound_call.SubsystemHint).ends_with("fos")) {
            continue;
        }

        if (nptr<AngelScript::asIScriptFunction> nullable_func = ResolveInboundRemoteCallImplementation(as_module, *meta, inbound_call); nullable_func) {
            if (backend->HasGameEngine()) {
                ptr<BaseEngine> engine = backend->GetGameEngine();
                auto func = nullable_func.as_ptr();
                engine->SetRemoteCallHandler(inbound_call.Name, [&inbound_call, engine, func](hstring name, nptr<Entity> entity, span<uint8_t> data) FO_DEFERRED {
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

        if (nptr<const AngelScript::asIScriptFunction> nullable_func = ResolveInboundRemoteCallImplementation(mod, meta, inbound_call); nullable_func) {
            auto func = nullable_func.as_ptr();

            if (std::ranges::find(matched_funcs, func) == matched_funcs.end()) {
                matched_funcs.emplace_back(func);
            }
        }
    }

    const auto append_error = [&errors](optional<pair<string, uint32_t>> location, const string& error) {
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
            const auto func_decl = GetFunctionDeclarationString(func);
            const auto message = strex("Inbound ///@ RemoteCall implementation '{}' must be marked [[{}]]", func_decl, expected_attr).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
    }

    for (ptr<const AngelScript::asIScriptFunction> func : CollectModuleScriptFunctions(mod)) {
        if (HasFunctionAttribute(func.get(), opposite_attr)) {
            const auto func_decl = GetFunctionDeclarationString(func);
            const auto message = strex("Functions marked [[{}]] must correspond to inbound ///@ RemoteCall declarations, '{}' uses the wrong remote-call attribute for this engine side", opposite_attr, func_decl).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
        else if (HasFunctionAttribute(func.get(), expected_attr) && std::ranges::find(matched_funcs, func) == matched_funcs.end()) {
            const auto func_decl = GetFunctionDeclarationString(func);
            const auto message = strex("Functions marked [[{}]] must correspond to inbound ///@ RemoteCall declarations, '{}' has no matching ///@ RemoteCall declaration", expected_attr, func_decl).str();
            append_error(ResolveDeclaredFunctionSourceLocation(func, lnt), message);
        }
    }

    return errors;
}

FO_END_NAMESPACE

#endif
