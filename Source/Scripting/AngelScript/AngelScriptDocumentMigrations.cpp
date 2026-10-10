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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "AngelScriptDocumentMigrations.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptAttributes.h"
#include "AngelScriptBackend.h"
#include "AngelScriptHelpers.h"
#include "EngineBase.h"
#include "PropertiesSerializer.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

namespace AngelScriptDocumentMigrations
{
    class ScriptDatabaseDocument final
    {
    public:
        ScriptDatabaseDocument(ptr<AngelScript::asIScriptEngine> as_engine, ptr<const PropertyRegistrar> registrar, const AnyData::Document& document) :
            _asEngine {as_engine},
            _registrar {registrar},
            _document {&document}
        {
        }

        [[nodiscard]] auto GetEntityType() const -> string { return string {_registrar->GetTypeName().as_str()}; }
        [[nodiscard]] auto ReadEncoded(const string& key) const -> string;
        auto Read(const string& key, ptr<void> output, int32_t output_type_id) const -> bool;

    private:
        ptr<AngelScript::asIScriptEngine> _asEngine;
        ptr<const PropertyRegistrar> _registrar;
        ptr<const AnyData::Document> _document;
    };

    static void ReadDocument(AngelScript::asIScriptGeneric* raw_generic);
    static auto FindMigrator(ptr<AngelScript::asIScriptModule> module, string_view name, string_view attribute, string_view value_type) -> ptr<AngelScript::asIScriptFunction>;
    static auto RunMigrator(ptr<AngelScriptBackend> backend, ptr<AngelScript::asIScriptFunction> function, ptr<void> value, ScriptDatabaseDocument& document) -> bool;

    void RegisterTypes(ptr<AngelScript::asIScriptEngine> as_engine)
    {
        FO_TRACE_ZONE(Script);

        int32_t as_result = 0;

        // No factory or handles: a document reference cannot escape its synchronous invocation
        FO_AS_VERIFY(as_engine->RegisterObjectType("DatabaseDocument", 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOHANDLE));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("DatabaseDocument", "string get_EntityType() const property", FO_SCRIPT_METHOD(ScriptDatabaseDocument, GetEntityType), FO_SCRIPT_METHOD_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("DatabaseDocument", "string ReadEncoded(const string&in) const", FO_SCRIPT_METHOD(ScriptDatabaseDocument, ReadEncoded), FO_SCRIPT_METHOD_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("DatabaseDocument", "bool Read(const string&in, ?&out) const", FO_SCRIPT_GENERIC(ReadDocument), FO_SCRIPT_GENERIC_CONV));
    }

    void Validate(ptr<AngelScript::asIScriptModule> module, const EngineMetadata& meta)
    {
        FO_TRACE_ZONE(Script);

        if (meta.GetSide() != EngineSideKind::ServerSide) {
            return;
        }

        for (const auto& [owner, entity] : meta.GetEntityTypes()) {
            ignore_unused(owner);

            for (size_t index = 1; index < entity.PropRegistrar->GetPropertiesCount(); index++) {
                auto prop = entity.PropRegistrar->GetPropertyByIndexUnsafe(index);

                if (!prop->GetMigratorName().empty() && !prop->IsDisabled()) {
                    FO_VERIFY_AND_THROW(prop->IsPersistent(), "Property migrator requires a Persistent property", prop->GetName());
                    (void)FindMigrator(module, prop->GetMigratorName(), "PropertyMigrator", MakeScriptPropertyName(prop));
                }
            }
        }

        for (const auto& [owner, functions] : meta.GetProtoMigrators()) {
            ignore_unused(owner);

            for (const auto& [source, migrator] : functions) {
                ignore_unused(source);
                (void)FindMigrator(module, migrator.Name, "ProtoMigrator", "hstring");
            }
        }
    }

    void Bind(ptr<AngelScript::asIScriptEngine> as_engine)
    {
        FO_TRACE_ZONE(Script);

        auto backend = GetScriptBackend(as_engine);
        auto meta = GetEngineMetadata(as_engine);

        if (meta->GetSide() != EngineSideKind::ServerSide || !backend->HasGameEngine()) {
            return;
        }

        auto engine = backend->GetGameEngine();
        auto module = make_nptr(as_engine->GetModuleByIndex(0));
        FO_VERIFY_AND_THROW(module, "Missing document migration script module");

        for (const auto& [owner, entity] : meta->GetEntityTypes()) {
            ignore_unused(owner);
            auto registrar = entity.PropRegistrar.as_ptr();

            for (size_t index = 1; index < registrar->GetPropertiesCount(); index++) {
                auto prop = registrar->GetPropertyByIndexUnsafe(index);

                if (prop->GetMigratorName().empty() || prop->IsDisabled()) {
                    continue;
                }

                auto function = FindMigrator(module.as_ptr(), prop->GetMigratorName(), "PropertyMigrator", MakeScriptPropertyName(prop));

                prop->SetMigrator([backend, engine, as_engine, registrar, prop, function](const AnyData::Value& value, const AnyData::Document& document) mutable -> optional<AnyData::Value> FO_DEFERRED {
                    PropertyRawData raw;
                    PropertiesSerializer::LoadPropertyFromValue(prop, value, [&](const_span<uint8_t> bytes) { raw.Set(nptr<const void> {bytes.data()}, bytes.size()); }, engine->Hashes, *engine);

                    PropertyRawData storage;
                    auto address = storage.Alloc(CalcConstructAddrSpace(prop));
                    ConvertPropsToScriptObject(prop, raw, address, as_engine);
                    auto free_value = scope_exit([prop, address]() noexcept { safe_call([&] { FreeConstructAddrSpace(prop, address); }); });

                    ScriptDatabaseDocument context {as_engine, registrar, document};

                    if (!RunMigrator(backend, function, address, context)) {
                        return std::nullopt;
                    }

                    auto result = ConvertScriptToPropsObject(prop, address);

                    return PropertiesSerializer::SavePropertyToValue(prop, {result.GetPtrAs<uint8_t>().get(), result.GetSize()}, engine->Hashes, *engine);
                });
            }
        }

        for (const auto& [owner, functions] : meta->GetProtoMigrators()) {
            auto registrar = meta->GetPropertyRegistrar(owner);
            FO_VERIFY_AND_THROW(registrar, "Prototype migrator owner does not exist", owner);

            for (const auto& [source, migrator] : functions) {
                auto function = FindMigrator(module.as_ptr(), migrator.Name, "ProtoMigrator", "hstring");

                engine->BindProtoMigrator(owner, source, [backend, as_engine, registrar = registrar.as_ptr(), function](hstring value, const AnyData::Document& document) -> optional<hstring> FO_DEFERRED {
                    ScriptDatabaseDocument context {as_engine, registrar, document};

                    if (!RunMigrator(backend, function, make_ptr(&value).void_cast(), context)) {
                        return std::nullopt;
                    }

                    return value;
                });
            }
        }
    }

    auto ScriptDatabaseDocument::Read(const string& key, ptr<void> output, int32_t output_type_id) const -> bool
    {
        FO_TRACE_ZONE(Script);

        auto declaration = _asEngine->GetTypeDeclaration(output_type_id, true);
        FO_VERIFY_AND_THROW(declaration, "Document output type has no declaration", output_type_id);
        string output_type = NormalizeScriptPropertyDecl(declaration);

        if (!key.empty() && (key[0] == '_' || key[0] == '$')) {
            FO_VERIFY_AND_THROW(output_type == "string", "Technical document reads require a string", key);

            if (!_document->Contains(key)) {
                return false;
            }

            const auto& value = (*_document)[key];
            FO_VERIFY_AND_THROW(value.Type() == AnyData::ValueType::String, "Technical document field is not a string", key);
            *output.reinterpret_as<string>() = value.AsString();

            return true;
        }

        auto prop = _registrar->FindProperty(key);
        FO_VERIFY_AND_THROW(prop, "Database document property does not exist", _registrar->GetTypeName(), key);
        auto expected_type = MakeScriptPropertyName(prop.as_ptr());
        FO_VERIFY_AND_THROW(output_type_id == _asEngine->GetTypeIdByDecl(expected_type.c_str()), "Database document output type does not match property", key, output_type);

        nptr<const AnyData::Value> selected;

        if (_document->Contains(key)) {
            selected = &(*_document)[key];
        }
        else {
            for (const auto& [stored_key, value] : *_document) {
                if (_registrar->FindPersistedProperty(stored_key) == prop) {
                    FO_VERIFY_AND_THROW(!selected, "Ambiguous database document property", key);
                    selected = &value;
                }
            }
        }

        auto as_engine = _asEngine;
        auto engine = GetGameEngine(as_engine);
        auto meta = GetEngineMetadata(as_engine);
        PropertyRawData raw;

        if (selected) {
            PropertiesSerializer::LoadPropertyFromValue(prop.as_ptr(), *selected, [&](const_span<uint8_t> bytes) { raw.Set(nptr<const void> {bytes.data()}, bytes.size()); }, engine->Hashes, *engine);
        }
        else if (_document->Contains("_Proto")) {
            const auto& proto_value = (*_document)["_Proto"];
            FO_VERIFY_AND_THROW(proto_value.Type() == AnyData::ValueType::String, "Document prototype must be a string");

            auto proto = meta->GetProtoEntity(_registrar->GetTypeName(), meta->Hashes.to_hashed_string(proto_value.AsString()));

            if (!proto) {
                return false;
            }

            raw.Pass(proto->GetProperties()->GetRawData(prop.as_ptr()));
        }
        else {
            return false;
        }

        PropertyRawData storage;
        auto address = storage.Alloc(CalcConstructAddrSpace(prop.as_ptr()));
        ConvertPropsToScriptObject(prop.as_ptr(), raw, address, _asEngine);
        auto free_value = scope_exit([prop, address]() noexcept { safe_call([&] { FreeConstructAddrSpace(prop.as_ptr(), address); }); });

        if ((output_type_id & AngelScript::asTYPEID_OBJHANDLE) != 0) {
            auto type_info = _asEngine->GetTypeInfoById(output_type_id);
            auto previous = NativeDataProvider::ReadHandleSlot(output);
            auto replacement = NativeDataProvider::ReadHandleSlot(address);

            if (replacement) {
                as_engine->AddRefScriptObject(replacement.get(), type_info);
            }
            if (previous) {
                as_engine->ReleaseScriptObject(previous.get(), type_info);
            }

            NativeDataProvider::WriteHandleSlot(output, replacement);
        }
        else if ((output_type_id & AngelScript::asTYPEID_MASK_OBJECT) != 0) {
            auto type_info = _asEngine->GetTypeInfoById(output_type_id);
            auto source = (type_info->GetFlags() & AngelScript::asOBJ_REF) != 0 ? NativeDataProvider::ReadHandleSlot(address).as_ptr() : address;
            int32_t as_result = 0;
            FO_AS_VERIFY(as_engine->AssignScriptObject(output.get(), source.get(), type_info));
        }
        else {
            memory::copy(output, address, CalcConstructAddrSpace(prop.as_ptr()));
        }

        return true;
    }

    auto ScriptDatabaseDocument::ReadEncoded(const string& key) const -> string
    {
        FO_VERIFY_AND_THROW(_document->Contains(key), "Database document field does not exist", key);

        return AnyData::ValueToString((*_document)[key]);
    }

    static void ReadDocument(AngelScript::asIScriptGeneric* raw_generic)
    {
        auto generic = make_ptr(raw_generic);
        auto document = GetGenericObjectAs<const ScriptDatabaseDocument>(generic);
        auto key = GetGenericArgAddressAs<const string>(generic, 0).as_ptr();
        auto output = GetGenericArgAddress(generic, 1).as_ptr();
        bool read = document->Read(*key, output, generic->GetArgTypeId(1));

        generic->SetReturnByte(read);
    }

    static auto FindMigrator(ptr<AngelScript::asIScriptModule> module, string_view name, string_view attribute, string_view value_type) -> ptr<AngelScript::asIScriptFunction>
    {
        nptr<AngelScript::asIScriptFunction> selected;

        for (AngelScript::asUINT index = 0; index < module->GetFunctionCount(); index++) {
            auto function = make_ptr(module->GetFunctionByIndex(index));
            string_view function_name = function->GetName();
            string_view name_space = function->GetNamespace();
            string qualified_name = name_space.empty() ? string {function_name} : strex("{}::{}", name_space, function_name).str();

            if (name != qualified_name && (name.find("::") != string_view::npos || name != function_name)) {
                continue;
            }

            FO_VERIFY_AND_THROW(!selected, "Document migrator function is ambiguous", name);
            selected = function;
        }

        FO_VERIFY_AND_THROW(selected, "Document migrator function does not exist", name);
        FO_VERIFY_AND_THROW(selected->GetObjectType() == nullptr && selected->GetFuncType() == AngelScript::asFUNC_SCRIPT && HasFunctionAttribute(selected.as_ptr(), attribute), "Document migrator must be an attributed global script function", name, attribute);
        FO_VERIFY_AND_THROW(!HasFunctionAttribute(selected.as_ptr(), "Async"), "Document migrator must be synchronous", name);
        FO_VERIFY_AND_THROW(selected->GetParamCount() == 2 && selected->GetReturnTypeId() == AngelScript::asTYPEID_BOOL, "Document migrator must return bool and take two arguments", name);

        int32_t type_id = 0;
        AngelScript::asDWORD flags = 0;
        int32_t as_result = 0;
        FO_AS_VERIFY(selected->GetParam(0, &type_id, &flags));
        string value_declaration {value_type};
        int32_t expected_type_id = module->GetEngine()->GetTypeIdByDecl(value_declaration.c_str());
        FO_VERIFY_AND_THROW(expected_type_id >= 0 && type_id == expected_type_id && flags == AngelScript::asTM_INOUTREF, "Document migrator value must match the property type and be inout", name, value_type);

        FO_AS_VERIFY(selected->GetParam(1, &type_id, &flags));
        auto context_type = module->GetEngine()->GetTypeInfoById(type_id);
        auto document_type = module->GetEngine()->GetTypeInfoByDecl("DatabaseDocument");
        FO_VERIFY_AND_THROW(context_type && context_type == document_type && flags == (AngelScript::asTM_INOUTREF | AngelScript::asTM_CONST), "Document migrator requires const DatabaseDocument&inout", name);

        return selected.as_ptr();
    }

    static auto RunMigrator(ptr<AngelScriptBackend> backend, ptr<AngelScript::asIScriptFunction> function, ptr<void> value, ScriptDatabaseDocument& document) -> bool
    {
        auto manager = backend->GetContextMngr();
        FO_VERIFY_AND_THROW(manager, "Document migration requires a script context manager");

        auto context = manager->PrepareContext(function);
        auto generation = manager->GetContextGeneration(context);
        auto return_context = scope_exit([&]() noexcept { manager->ReturnContext(context, generation); });

        int32_t as_result = 0;
        FO_AS_VERIFY(context->SetArgAddress(0, value.get()));
        FO_AS_VERIFY(context->SetArgObject(1, &document));
        bool completed = manager->RunContext(context, false);
        FO_VERIFY_AND_THROW(completed, "Document migrator did not complete synchronously");

        return context->GetReturnByte() != 0;
    }
}

FO_END_NAMESPACE

#endif
