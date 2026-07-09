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

#include "AngelScriptEntity.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptArray.h"
#include "AngelScriptAttributes.h"
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptContext.h"
#include "AngelScriptHelpers.h"
#include "EngineBase.h"
#include "Entity.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

static void Entity_AddRef(const Entity* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    self->AddRef();
}

static void Entity_Release(const Entity* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    self->Release();
}

static auto Entity_IsDestroyed(const Entity* self) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return self->IsDestroyed();
}

static auto Entity_IsDestroying(const Entity* self) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return self->IsDestroying();
}

static auto Entity_Name(const Entity* self) -> string
{
    FO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return string(self->GetName());
}

static auto Entity_Id(const Entity* self) -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return self->GetId();
}

static auto Entity_ProtoId(const Entity* self) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    CheckScriptEntityNonDestroyed(self);

    ptr<const Entity> self_ref = self;

    if (auto self_proto = self_ref.dyn_cast<const ProtoEntity>()) {
        return self_proto->GetProtoId();
    }
    if (auto self_with_proto = self_ref.dyn_cast<const EntityWithProto>()) {
        return self_with_proto->GetProtoId();
    }

    return {};
}

static auto ReturnScriptEntity(ptr<const Entity> entity) noexcept -> const Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    return entity.get();
}

static auto Entity_Proto(const Entity* self) -> const Entity*
{
    FO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    CheckScriptEntityNonDestroyed(self);

    ptr<const Entity> self_ref = self;

    if (auto self_proto = self_ref.dyn_cast<const ProtoEntity>()) {
        return ReturnScriptEntity(self_proto.as_ptr());
    }
    if (auto self_with_proto = self_ref.dyn_cast<const EntityWithProto>()) {
        ptr<const Entity> self_proto = self_with_proto->GetProto();
        return ReturnScriptEntity(self_proto);
    }

    return {};
}

static auto Entity_GetSelfForEvent(Entity* entity) -> Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return entity;
}

static auto Entity_GetValueAsInt(const Entity* entity, int32_t prop_index) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityAccessAndNonDestroyed(entity);

    auto prop = entity->GetProperties()->GetRegistrator()->GetPropertyByIndex(prop_index);

    if (!prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    return entity->GetValueAsInt(prop.as_ptr());
}

static void Entity_SetValueAsInt(Entity* entity, int32_t prop_index, int32_t value)
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityAccessAndNonDestroyed(entity);

    auto prop = entity->GetProperties()->GetRegistrator()->GetPropertyByIndex(prop_index);

    if (!prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (!prop->IsMutable()) {
        throw ScriptException("Property is not mutable");
    }

    entity->SetValueAsInt(prop, value);
}

static auto Entity_GetValueAsAny(const Entity* entity, int32_t prop_index) -> any_t
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityAccessAndNonDestroyed(entity);

    auto prop = entity->GetProperties()->GetRegistrator()->GetPropertyByIndex(prop_index);

    if (!prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    return entity->GetValueAsAny(prop.as_ptr());
}

static void Entity_SetValueAsAny(Entity* entity, int32_t prop_index, any_t value)
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityAccessAndNonDestroyed(entity);

    auto prop = entity->GetProperties()->GetRegistrator()->GetPropertyByIndex(prop_index);

    if (!prop) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (!prop->IsMutable()) {
        throw ScriptException("Property is not mutable");
    }

    entity->SetValueAsAny(prop, value);
}

static void Entity_GetComponent(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(entity);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);
    const auto props = entity->GetProperties();
    const auto has_component = props->GetValue<bool>(prop);

    if (!has_component) {
        throw ScriptException("Component is not present on entity (check the Has-accessor first)", prop->GetName());
    }

    ReturnGenericEntity(gen, entity);
}

static void Entity_HasComponent(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    // May call on unsynced entity
    CheckScriptEntityNonDestroyed(entity);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);
    const auto props = entity->GetProperties();

    new (gen->GetAddressOfReturnLocation()) bool(props->GetValue<bool>(prop));
}

static void Entity_GetPropertyValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityNonNull(entity);

    entity->LockForPropertyAccessShared();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccessShared(); });

    CheckScriptEntityAccessAndNonDestroyed(entity);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);
    auto getter = prop->GetGetter();

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        if (!*getter) {
            throw ScriptException("Getter not set");
        }

        prop_data = (*getter)(entity.get(), prop.get());
    }
    else {
        const auto props = entity->GetProperties();
        props->ValidateForRawData(prop);
        props->CopyRawData(prop, prop_data);
    }

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    ConvertPropsToScriptObject(prop, prop_data, gen->GetAddressOfReturnLocation(), as_engine);
}

static void Entity_SetPropertyValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityNonNull(entity);

    entity->LockForPropertyAccess();
    auto auto_unlock = scope_exit([entity]() mutable noexcept { entity->UnlockForPropertyAccess(); });

    CheckScriptEntityAccessAndNonDestroyed(entity);
    auto prop = GetGenericAuxiliaryAs<const Property>(gen);
    auto props = entity->GetPropertiesForEdit();
    auto as_obj = GetGenericAddressArg(gen, 0);

    auto prop_data = ConvertScriptToPropsObject(prop, as_obj);
    props->SetValue(prop, prop_data);
}

static auto Entity_DownCast(Entity* entity) -> Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on unsynced entity
    // May call on destroyed entity
    return entity;
}

static void Entity_UpCast(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    // May call on unsynced entity
    // May call on destroyed entity
    const auto entity_type_name = entity->GetTypeName();
    auto target_type_name = GetGenericAuxiliaryAs<const string>(gen);
    bool valid_cast = false;

    if (target_type_name->find(entity_type_name) != string::npos) {
        if (target_type_name->starts_with("Proto")) {
            valid_cast = !!entity.dyn_cast<ProtoEntity>();
        }
        else if (target_type_name->starts_with("Static")) {
            valid_cast = !entity->GetId();
        }
        else if (target_type_name->starts_with("Abstract")) {
            valid_cast = true;
        }
        else {
            FO_VERIFY_AND_THROW(*target_type_name == entity_type_name.as_str(), "Event target type name does not match entity type");
            valid_cast = !entity.dyn_cast<ProtoEntity>() && !!entity->GetId();
        }
    }

    if (valid_cast) {
        ReturnGenericEntity(gen, entity);
    }
    else {
        ReturnGenericEntity(gen, nullptr);
    }
}

static void Game_GetProtoCustomEntity(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto engine = GetGameEngine(as_engine);
    auto pid = GetGenericAddressArgAs<const hstring>(gen, 0);
    const auto entity_hname = engine->Hashes.ToHashedString(*entity_name);
    nptr<const ProtoEntity> proto = engine->GetProtoEntity(entity_hname, *pid);

    if (!proto) {
        throw ScriptException("Proto entity not found (check the Check-accessor first)", *entity_name, *pid);
    }

    auto casted_proto = proto.dyn_cast<const ProtoCustomEntity>();
    FO_VERIFY_AND_THROW(casted_proto, "Prototype is not a custom entity prototype");
    ptr<ProtoCustomEntity> mutable_proto = ScriptMutablePtr(casted_proto);
    ReturnGenericEntity(gen, mutable_proto);
}

static void Game_CheckProtoCustomEntity(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto engine = GetGameEngine(as_engine);
    auto pid = GetGenericAddressArgAs<const hstring>(gen, 0);
    const auto entity_hname = engine->Hashes.ToHashedString(*entity_name);
    const nptr<const ProtoEntity> proto = engine->GetProtoEntity(entity_hname, *pid);

    new (gen->GetAddressOfReturnLocation()) bool(proto);
}

static void Game_GetProtoCustomEntities(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto engine = GetGameEngine(as_engine);
    const auto entity_type = engine->Hashes.ToHashedString(*entity_name);
    const auto& protos = engine->GetProtoEntities(entity_type);
    const bool is_fixed_type = engine->IsFixedType(entity_type);

    auto result = CreateScriptArray(as_engine, is_fixed_type ? strex("array<{}>", entity_type).c_str() : strex("array<Proto{}>", entity_type).c_str());
    result->Reserve(numeric_cast<int32_t>(protos.size()));

    for (auto proto_it = protos.cbegin(); proto_it != protos.cend(); ++proto_it) {
        ptr<Entity> entity = proto_it->second.get_no_const();
        nptr<Entity> entity_arg = entity;
        auto value = make_ptr(entity_arg.get_pp()).reinterpret_as<void>();
        result->InsertLast(value);
    }

    ReturnGenericScriptArray(gen, std::move(result));
}

static void Game_GetProtoCustomEntitiesByProperty(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto engine = GetGameEngine(as_engine);
    const auto entity_type = engine->Hashes.ToHashedString(*entity_name);
    const auto prop_enum = static_cast<int32_t>(*GetGenericAddressArgAs<ScriptEnum_uint16>(gen, 0));
    auto prop_value = GetGenericAddressArgAs<const any_t>(gen, 1);
    auto registrator = engine->GetPropertyRegistrator(*entity_name);
    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");
    auto prop = registrator->GetPropertyByIndex(prop_enum);

    if (!prop) {
        throw ScriptException("Property invalid enum", prop_enum);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property is not a plain-data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    const auto& protos = engine->GetProtoEntities(entity_type);

    auto result = CreateScriptArray(as_engine, strex("array<{}>", entity_type).c_str());

    for (auto proto_it = protos.cbegin(); proto_it != protos.cend(); ++proto_it) {
        auto proto = proto_it->second.as_ptr();

        if (proto->GetValueAsAny(prop) == *prop_value) {
            ptr<Entity> entity = proto_it->second.get_no_const();
            nptr<Entity> entity_arg = entity;
            auto value = make_ptr(entity_arg.get_pp()).reinterpret_as<void>();
            result->InsertLast(value);
        }
    }

    ReturnGenericScriptArray(gen, std::move(result));
}

static void Game_GetEntity(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto backend = GetScriptBackend(as_engine);
    auto id = GetGenericAddressArgAs<const ident_t>(gen, 0);
    const auto entity_hname = backend->GetMetadata()->Hashes.ToHashedString(*entity_name);
    auto entity_mngr = backend->GetEntityMngr();

    auto custom_entity = entity_mngr->GetCustomEntity(entity_hname, *id);

    if (custom_entity) {
        ReturnGenericEntity(gen, custom_entity);
    }
    else {
        ReturnGenericEntity(gen, nullptr);
    }
}

static void Game_DestroyOne(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    auto entity_mngr = backend->GetEntityMngr();
    auto entity = NativeDataProvider::ReadTypedHandleSlot<Entity>(GetGenericAddressArg(gen, 0));

    if (entity) {
        entity_mngr->DestroyEntity(entity);
    }
}

static void Game_DestroyAll(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    auto entity_mngr = backend->GetEntityMngr();
    auto entities = NativeDataProvider::ReadTypedHandleSlot<ScriptArray>(GetGenericAddressArg(gen, 0));
    FO_VERIFY_AND_THROW(entities, "Entity array argument is null");

    for (int32_t i = 0; i < entities->GetSize(); i++) {
        auto entity = entities->AtAs<Entity>(i);

        if (entity) {
            entity_mngr->DestroyEntity(entity);
        }
    }
}

static void CustomEntity_Add(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto entry = GetGenericAuxiliaryAs<const hstring>(gen);
    auto holder = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(holder);
    const auto pid = gen->GetArgCount() == 1 ? *GetGenericAddressArgAs<const hstring>(gen, 0) : hstring();
    auto backend = GetScriptBackend(as_engine);
    auto entity_mngr = backend->GetEntityMngr();

    auto entity = entity_mngr->CreateCustomInnerEntity(holder, *entry, pid);
    ReturnGenericEntity(gen, entity);
}

static void CustomEntity_HasAny(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entry = GetGenericAuxiliaryAs<const hstring>(gen);
    auto holder = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(holder);
    const nptr<const vector<refcount_ptr<Entity>>> entities = holder->GetInnerEntities(*entry);

    new (gen->GetAddressOfReturnLocation()) bool(!!entities);
}

static void CustomEntity_GetOne(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entry = GetGenericAuxiliaryAs<const hstring>(gen);
    auto holder = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(holder);
    const ident_t id = *GetGenericAddressArgAs<const ident_t>(gen, 0);
    nptr<vector<refcount_ptr<Entity>>> entities = holder->GetInnerEntities(*entry);

    if (entities && !entities->empty()) {
        size_t entity_index = entities->size();

        for (size_t i = 0; i < entities->size(); i++) {
            auto entity = (*entities)[i].as_ptr();
            CheckScriptEntityAccessAndNonDestroyed(entity);

            if (entity->GetId() == id) {
                entity_index = i;
                break;
            }
        }

        if (entity_index != entities->size()) {
            ReturnGenericEntity(gen, (*entities)[entity_index]);
        }
        else {
            ReturnGenericEntity(gen, nullptr);
        }
    }
    else {
        ReturnGenericEntity(gen, nullptr);
    }
}

static void CustomEntity_GetAll(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entry = GetGenericAuxiliaryAs<const hstring>(gen);
    auto holder = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(holder);
    nptr<vector<refcount_ptr<Entity>>> entities = holder->GetInnerEntities(*entry);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto meta = GetEngineMetadata(as_engine);
    const auto& holder_type = meta->GetEntityType(holder->GetTypeName());
    auto holder_entry = make_ptr(&holder_type.HolderEntries.at(*entry));

    if (entities && !entities->empty()) {
        vector<ptr<Entity>> result_entities;
        result_entities.reserve(entities->size());

        for (size_t i = 0; i < entities->size(); i++) {
            auto entity = (*entities)[i].as_ptr();
            CheckScriptEntityNonNull(entity);
            CheckScriptEntityAccessAndNonDestroyed(entity);
            result_entities.emplace_back(entity);
        }

        auto arr = CreateScriptArray(as_engine, strex("array<{}>", holder_entry->TargetType).c_str());
        arr->Reserve(numeric_cast<int32_t>(result_entities.size()));

        for (ptr<Entity> result_entity : result_entities) {
            nptr<Entity> result_entity_arg = result_entity;
            auto value = make_ptr(result_entity_arg.get_pp()).reinterpret_as<void>();
            arr->InsertLast(value);
        }

        ReturnGenericScriptArray(gen, std::move(arr));
    }
    else {
        auto arr = CreateScriptArray(as_engine, strex("array<{}>", holder_entry->TargetType).c_str());
        ReturnGenericScriptArray(gen, std::move(arr));
    }
}

static void Game_SetPropertyGetter(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    auto registrator = backend->GetMetadata()->GetPropertyRegistrator(*entity_name);
    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");
    const auto prop_enum = *GetGenericAddressArgAs<ScriptEnum_uint16>(gen, 0);

    if (static_cast<int32_t>(prop_enum) == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    auto prop = registrator->GetPropertyByIndex(static_cast<int32_t>(prop_enum));
    FO_VERIFY_AND_THROW(prop, "Missing property instance");

    nptr<const AngelScript::asITypeInfo> as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1));
    nptr<const AngelScript::asIScriptFunction> funcdef {};

    if (as_type_info) {
        funcdef = as_type_info->GetFuncdefSignature();
    }

    if (!funcdef) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(GetGenericArgAddress(gen, 1).as_ptr());

    if (!func) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    if (!prop->IsVirtual()) {
        throw ScriptException("Property is not virtual", prop->GetName());
    }
    if (func->GetParamCount() != 1 && func->GetParamCount() != 2) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() == AngelScript::asTYPEID_VOID) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    const nptr<const char> return_type_decl = as_engine->GetTypeDeclaration(func->GetReturnTypeId());
    if (MakeScriptPropertyName(prop) != NormalizeScriptPropertyDecl(return_type_decl ? string_view {return_type_decl.get()} : string_view {})) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    int32_t type_id;
    AngelScript::asDWORD flags;
    int32_t as_result;
    FO_AS_VERIFY(func->GetParam(0, &type_id, &flags));

    nptr<const AngelScript::asITypeInfo> param_type_info = as_engine->GetTypeInfoById(type_id);
    if (!param_type_info || flags != 0) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    if (string_view(param_type_info->GetName()) != *entity_name) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    if (func->GetParamCount() == 2) {
        FO_AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (type_id != gen->GetArgTypeId(0) || flags != 0) {
            throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
        }
    }

    prop->SetGetter([=](nptr<Entity> entity, ptr<const Property>) mutable -> PropertyRawData FO_DEFERRED {
        CheckScriptEntityAccessAndNonDestroyed(entity);

        auto context_mngr = backend->GetContextMngr();
        FO_VERIFY_AND_THROW(context_mngr, "Missing script context manager");
        auto ctx = context_mngr->PrepareContext(func.as_ptr());
        const uint64_t ctx_generation = context_mngr->GetContextGeneration(ctx);
        auto return_ctx = scope_exit([&, ctx_generation]() noexcept { context_mngr->ReturnContext(ctx, ctx_generation); });
        ctx->SetArgObject(0, entity.get()); // May be null for protos

        if (func->GetParamCount() == 2) {
            ctx->SetArgWord(1, prop->GetRegIndex());
        }

        const auto run_ok = context_mngr->RunContext(ctx, false);
        FO_VERIFY_AND_THROW(run_ok, "Script context execution failed");

        auto prop_data = ConvertScriptToPropsObject(prop.as_ptr(), ctx->GetAddressOfReturnValue());
        prop_data.StoreIfPassed();
        return prop_data;
    });
}

static void Game_AddPropertySetter(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    auto registrator = backend->GetMetadata()->GetPropertyRegistrator(*entity_name);
    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");
    const auto prop_enum = *GetGenericAddressArgAs<ScriptEnum_uint16>(gen, 0);

    if (static_cast<int32_t>(prop_enum) == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    auto prop = registrator->GetPropertyByIndex(static_cast<int32_t>(prop_enum));
    FO_VERIFY_AND_THROW(prop, "Missing property instance");

    nptr<const AngelScript::asITypeInfo> as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1));
    nptr<const AngelScript::asIScriptFunction> funcdef {};

    if (as_type_info) {
        funcdef = as_type_info->GetFuncdefSignature();
    }

    if (!funcdef) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(GetGenericArgAddress(gen, 1).as_ptr());

    if (!func) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    if (func->GetParamCount() != 1 && func->GetParamCount() != 2 && func->GetParamCount() != 3) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    int32_t type_id;
    AngelScript::asDWORD flags;
    FO_AS_VERIFY(func->GetParam(0, &type_id, &flags));

    nptr<const AngelScript::asITypeInfo> param_type_info = as_engine->GetTypeInfoById(type_id);
    if (!param_type_info || flags != 0) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    if (string_view(param_type_info->GetName()) != *entity_name) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    bool has_proto_enum = false;
    bool has_value_ref = false;

    if (func->GetParamCount() > 1) {
        FO_AS_VERIFY(func->GetParam(1, &type_id, &flags));

        const nptr<const char> param_type_decl = as_engine->GetTypeDeclaration(type_id);
        if (MakeScriptPropertyName(prop) == NormalizeScriptPropertyDecl(param_type_decl ? string_view {param_type_decl.get()} : string_view {}) && flags == AngelScript::asTM_INOUTREF) {
            has_value_ref = true;
            if (func->GetParamCount() == 3) {
                throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
            }
        }
        else if (type_id == gen->GetArgTypeId(0) && flags == 0) {
            has_proto_enum = true;
        }
        else {
            throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
        }

        if (func->GetParamCount() == 3) {
            FO_AS_VERIFY(func->GetParam(2, &type_id, &flags));

            const nptr<const char> value_param_type_decl = as_engine->GetTypeDeclaration(type_id);
            if (MakeScriptPropertyName(prop) == NormalizeScriptPropertyDecl(value_param_type_decl ? string_view {value_param_type_decl.get()} : string_view {}) && flags == AngelScript::asTM_INOUTREF) {
                has_value_ref = true;
            }
            else {
                throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
            }
        }
    }

    if (has_value_ref) {
        // Value-transforming setter: runs before the value is stored so it can rewrite prop_data.
        prop->AddSetter([=](nptr<Entity> entity, ptr<const Property>, PropertyRawData& prop_data) mutable FO_DEFERRED {
            int32_t as_result = 0;
            FO_VERIFY_AND_THROW(entity, "Property setter target entity is null");
            CheckScriptEntityAccessAndNonDestroyed(entity);

            auto context_mngr = backend->GetContextMngr();
            FO_VERIFY_AND_THROW(context_mngr, "Missing script context manager");
            auto ctx = context_mngr->PrepareContext(func.as_ptr());
            const uint64_t ctx_generation = context_mngr->GetContextGeneration(ctx);
            auto return_ctx = scope_exit([&, ctx_generation]() noexcept { context_mngr->ReturnContext(ctx, ctx_generation); });

            FO_AS_VERIFY(ctx->SetArgObject(0, entity.get()));

            if (has_proto_enum) {
                FO_AS_VERIFY(ctx->SetArgWord(1, prop->GetRegIndex()));
            }

            PropertyRawData value_ref_space;
            ptr<void> construct_addr = value_ref_space.Alloc(CalcConstructAddrSpace(prop));
            ConvertPropsToScriptObject(prop, prop_data, construct_addr, as_engine);
            FO_AS_VERIFY(ctx->SetArgAddress(has_proto_enum ? 2 : 1, construct_addr.get()));

            const bool run_ok = context_mngr->RunContext(ctx, false);
            FO_VERIFY_AND_THROW(run_ok, "Script context execution failed");

            prop_data = ConvertScriptToPropsObject(prop, construct_addr);
            prop_data.StoreIfPassed();
            FreeConstructAddrSpace(prop, construct_addr);
        });
    }
    else {
        // React-only setter: runs after the value is committed (post-setter), so the callback sees the new value
        // and executes inside the writing caller's lock cover.
        prop->AddPostSetter([=](nptr<Entity> entity, ptr<const Property>) mutable FO_DEFERRED {
            int32_t as_result = 0;
            FO_VERIFY_AND_THROW(entity, "Property setter target entity is null");
            CheckScriptEntityAccessAndNonDestroyed(entity);

            auto context_mngr = backend->GetContextMngr();
            FO_VERIFY_AND_THROW(context_mngr, "Missing script context manager");
            auto ctx = context_mngr->PrepareContext(func.as_ptr());
            const uint64_t ctx_generation = context_mngr->GetContextGeneration(ctx);
            auto return_ctx = scope_exit([&, ctx_generation]() noexcept { context_mngr->ReturnContext(ctx, ctx_generation); });

            FO_AS_VERIFY(ctx->SetArgObject(0, entity.get()));

            if (has_proto_enum) {
                FO_AS_VERIFY(ctx->SetArgWord(1, prop->GetRegIndex()));
            }

            context_mngr->RunContext(ctx, true);
        });
    }
}

static void Game_GetPropertyInfo(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);
    const auto prop_enum = static_cast<int32_t>(*GetGenericAddressArgAs<ScriptEnum_uint16>(gen, 0));
    auto is_disabled = GetGenericArgAddressAs<bool>(gen, 1);
    auto is_virtual = GetGenericArgAddressAs<bool>(gen, 2);
    auto is_dict = GetGenericArgAddressAs<bool>(gen, 3);
    auto is_array = GetGenericArgAddressAs<bool>(gen, 4);
    auto is_string_like = GetGenericArgAddressAs<bool>(gen, 5);
    auto enum_name = GetGenericArgAddressAs<string>(gen, 6);
    auto is_int = GetGenericArgAddressAs<bool>(gen, 7);
    auto is_float = GetGenericArgAddressAs<bool>(gen, 8);
    auto is_bool = GetGenericArgAddressAs<bool>(gen, 9);
    auto base_size = GetGenericArgAddressAs<int32_t>(gen, 10);
    auto is_synced = GetGenericArgAddressAs<bool>(gen, 11);

    if (prop_enum == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    auto entity_name = GetGenericAuxiliaryAs<const string>(gen);
    auto registrator = engine->GetPropertyRegistrator(*entity_name);
    FO_VERIFY_AND_THROW(registrator, "Missing property registrator");
    auto prop = registrator->GetPropertyByIndex(prop_enum);
    FO_VERIFY_AND_THROW(prop, "Missing property instance");

    *is_disabled = prop->IsDisabled();
    *is_virtual = prop->IsVirtual();
    *is_dict = prop->IsDict();
    *is_array = prop->IsArray();
    *is_string_like = prop->IsBaseTypeString() || prop->IsBaseTypeHash();
    *enum_name = prop->IsBaseTypeEnum() ? prop->GetBaseTypeName() : "";
    *is_int = prop->IsBaseTypeInt();
    *is_float = prop->IsBaseTypeFloat();
    *is_bool = prop->IsBaseTypeBool();
    *base_size = numeric_cast<int32_t>(prop->GetBaseSize());
    *is_synced = prop->IsSynced();
}

static void Entity_MethodCall(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto entity = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(entity);
    auto method = GetGenericAuxiliaryAs<const MethodDesc>(gen);
    FO_VERIFY_AND_THROW(method->Call, "Method call binding is null");

    ScriptGenericCall(gen, true, method->Args, [&](FuncCallData& call) { method->Call(call); });
}

static void Entity_GlobalMethodCall(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);

    auto method = GetGenericAuxiliaryAs<const MethodDesc>(gen);
    FO_VERIFY_AND_THROW(method->Call, "Method call binding is null");

    ScriptGenericCall(gen, false, method->Args, [&](FuncCallData& base_call) {
        FuncCallData call = base_call;
        nptr<Entity> engine_arg = engine;
        vector<ptr<void>> args_data;
        args_data.reserve(base_call.ArgsData.size() + 1);

        args_data.emplace_back(make_ptr(engine_arg.get_pp()).void_cast());

        for (size_t i = 0; i < base_call.ArgsData.size(); i++) {
            args_data.emplace_back(base_call.ArgsData[i]);
        }

        call.ArgsData = const_span<ptr<void>> {args_data.data(), args_data.size()};
        method->Call(call);
    });
}

static void ValidateCallbackFunc(nptr<AngelScript::asIScriptFunction> func)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto resolve_callback_func = [](nptr<AngelScript::asIScriptFunction> callback) noexcept -> nptr<AngelScript::asIScriptFunction> {
        if (callback && callback->GetFuncType() == AngelScript::asFUNC_DELEGATE) {
            if (auto delegate_func = callback->GetDelegateFunction(); delegate_func) {
                return delegate_func;
            }
        }

        return callback;
    };

    auto callback_func = resolve_callback_func(func);

    if (!callback_func) {
        throw ScriptException("Null callback passed to event");
    }
    if (!HasFunctionAttribute(callback_func.get(), "Event")) {
        throw ScriptException(strex("Only functions marked [[Event]] can be passed to events, got '{}'", callback_func->GetDeclaration(true, true, false)).str());
    }
}

static void EntityEvent_Subscribe(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto event = GetGenericAuxiliaryAs<const EntityEventDesc>(gen);
    auto entity = GetGenericObjectAs<Entity>(gen);
    CheckScriptEntityAccessAndNonDestroyed(entity);
    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(GetGenericAddressArg(gen, 0));
    ValidateCallbackFunc(func);

    const auto return_type_id = func->GetReturnTypeId();
    ptr<AngelScript::asIScriptEngine> as_engine = func->GetEngine();
    nptr<const AngelScript::asITypeInfo> return_type = return_type_id != AngelScript::asTYPEID_VOID ? as_engine->GetTypeInfoById(return_type_id) : nullptr;
    FO_VERIFY_AND_THROW(return_type_id == AngelScript::asTYPEID_VOID || (return_type && return_type->GetName() == string_view("EventResult")), "Entity event callback has unsupported return type", return_type_id);
    auto priority = GetGenericAddressArgAs<const Entity::EventPriority>(gen, 1);

    auto func_desc = IndexScriptFunc(func.as_ptr());
    FO_VERIFY_AND_THROW(func_desc->Call, "Script function descriptor has no native call handler");

    Entity::EventCallbackData event_data;

    event_data.Callback = [func_ = refcount_ptr<AngelScript::asIScriptFunction>::from_add_ref(func.get())](FuncCallData& call) mutable -> Entity::EventResult FO_DEFERRED {
        const bool event_has_result = func_->GetReturnTypeId() != AngelScript::asTYPEID_VOID;
        Entity::EventResult event_result = Entity::EventResult::ContinueChain;
        call.RetData = event_has_result ? make_nptr(&event_result).void_cast() : nullptr;
        ScriptFuncCall(func_, call);
        return event_has_result ? event_result : Entity::EventResult::ContinueChain;
    };

    event_data.SubscriptionPtr = std::bit_cast<uintptr_t>(func.get());
    event_data.Priority = *priority;
    event_data.HasExplicitResult = func->GetReturnTypeId() != AngelScript::asTYPEID_VOID;

    entity->SubscribeEvent(event->Name, std::move(event_data));
}

static void EntityEvent_Unsubscribe(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto event = GetGenericAuxiliaryAs<const EntityEventDesc>(gen);
    auto entity = GetGenericObjectAs<Entity>(gen);
    auto func = NativeDataProvider::ReadTypedHandleSlot<AngelScript::asIScriptFunction>(GetGenericAddressArg(gen, 0));
    ValidateCallbackFunc(func);

    // May call on unsynced entity
    // May call on destroyed entity
    if (entity->IsDestroyed()) {
        return;
    }

    entity->UnsubscribeEvent(event->Name, std::bit_cast<uintptr_t>(func.get()));
}

static void EntityEvent_UnsubscribeAll(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto event = GetGenericAuxiliaryAs<const EntityEventDesc>(gen);
    auto entity = GetGenericObjectAs<Entity>(gen);

    // May call on unsynced entity
    // May call on destroyed entity
    if (entity->IsDestroyed()) {
        return;
    }

    entity->UnsubscribeAllEvent(event->Name);
}

static void EntityEvent_Fire(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto event = GetGenericAuxiliaryAs<const EntityEventDesc>(gen);
    auto entity = GetGenericObjectAs<Entity>(gen);

    // May call on unsynced entity
    // May call on destroyed entity
    if (!entity->IsDestroyed() && entity->HasEventCallbacks(event->Name)) {
        ScriptGenericCall(gen, !entity->IsGlobal(), event->Args, [&](FuncCallData& call) {
            const auto result = entity->FireEvent(event->Name, call);
            new (gen->GetAddressOfReturnLocation()) Entity::EventResult(result);
        });
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity::EventResult(Entity::EventResult::ContinueChain);
    }
}

static void Game_SetConstGlobalVar(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AngelScript::asIScriptEngine> as_engine = gen->GetEngine();
    auto engine = GetGameEngine(as_engine);

    if (engine->IsGlobalVarsFrozen()) {
        throw ScriptException("SetConstGlobalVar is only allowed during module initialization");
    }

    const auto target_type_id = gen->GetArgTypeId(0);
    auto target_addr = GetGenericAddressArg(gen, 0);

    const auto value_type_id = gen->GetArgTypeId(1);
    auto value_addr = GetGenericAddressArg(gen, 1);

    const auto target_base = target_type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST);
    const auto value_base = value_type_id & ~(AngelScript::asTYPEID_OBJHANDLE | AngelScript::asTYPEID_HANDLETOCONST);

    if (target_base != value_base) {
        throw ScriptException("SetConstGlobalVar: type mismatch between target and value");
    }

    if (target_type_id & AngelScript::asTYPEID_OBJHANDLE) {
        nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoById(target_base);

        auto target_handle = NativeDataProvider::ReadIndirectHandleSlotPointer(target_addr);
        nptr<void> old_obj = *target_handle;

        nptr<void> new_obj {};

        if (value_type_id & AngelScript::asTYPEID_OBJHANDLE) {
            new_obj = NativeDataProvider::ReadIndirectHandleSlot(value_addr);
        }
        else if (value_type_id & AngelScript::asTYPEID_MASK_OBJECT) {
            new_obj = value_addr;
        }

        if (old_obj && type_info) {
            as_engine->ReleaseScriptObject(old_obj.get(), type_info.get());
        }
        if (new_obj && type_info) {
            as_engine->AddRefScriptObject(new_obj.get(), type_info.get());
        }

        *target_handle = new_obj.get();
    }
    else if (target_type_id & AngelScript::asTYPEID_MASK_OBJECT) {
        nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoById(target_type_id);
        FO_VERIFY_AND_THROW(type_info, "Missing target type info");

        nptr<void> src {};

        if (value_type_id & AngelScript::asTYPEID_OBJHANDLE) {
            src = NativeDataProvider::ReadIndirectHandleSlot(value_addr);
        }
        else {
            src = value_addr;
        }

        as_engine->AssignScriptObject(target_addr.get(), src.get(), type_info.get());
    }
    else {
        const auto size = as_engine->GetSizeOfPrimitiveType(target_type_id);

        if (size > 0) {
            std::memcpy(target_addr.get(), value_addr.get(), size);
        }
    }
}

void RegisterAngelScriptEntity(ptr<AngelScript::asIScriptEngine> as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32_t as_result = 0;
    auto backend = GetScriptBackend(as_engine);
    auto meta = backend->GetMetadata();
    FO_VERIFY_AND_THROW(meta, "Missing engine metadata");

    // Register entities
    const auto const_name = [&](const char* name) -> ptr<const string> {
        const auto hname = meta->Hashes.ToHashedString(name);
        return hname.as_str_ptr();
    };

    const auto register_base_entity = [&](const char* name) {
        FO_AS_VERIFY(as_engine->RegisterObjectType(name, 0, AngelScript::asOBJ_REF));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_FUNC_THIS(Entity_AddRef), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_FUNC_THIS(Entity_Release), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "bool get_IsDestroyed() const", FO_SCRIPT_FUNC_THIS(Entity_IsDestroyed), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "bool get_IsDestroying() const", FO_SCRIPT_FUNC_THIS(Entity_IsDestroying), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "string get_Name() const", FO_SCRIPT_FUNC_THIS(Entity_Name), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_cast = [&](string_view name, string_view base_name) {
        const string name_str(name);
        const string base_name_str(base_name);

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(base_name_str.c_str(), strex("{}@+ opCast()", name_str).c_str(), FO_SCRIPT_GENERIC(Entity_UpCast), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name_str.c_str()).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(base_name_str.c_str(), strex("const {}@+ opCast() const", name_str).c_str(), FO_SCRIPT_GENERIC(Entity_UpCast), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name_str.c_str()).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("{}@+ opImplCast()", base_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_DownCast), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("const {}@+ opImplCast() const", base_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_DownCast), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_getset = [&](string_view name, string_view prop_name) {
        const string name_str(name);
        const string prop_name_str(prop_name);

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("int GetAsInt({}Property prop) const", prop_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetValueAsInt), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("void SetAsInt({}Property prop, int value)", prop_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_SetValueAsInt), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("any GetAsAny({}Property prop) const", prop_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetValueAsAny), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name_str.c_str(), strex("void SetAsAny({}Property prop, any value)", prop_name_str).c_str(), FO_SCRIPT_FUNC_THIS(Entity_SetValueAsAny), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_props = [&](const char* name) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void SetPropertyGetter({}Property prop, ?&in func)", name).c_str(), FO_SCRIPT_GENERIC(Game_SetPropertyGetter), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void AddPropertySetter({}Property prop, ?&in func)", name).c_str(), FO_SCRIPT_GENERIC(Game_AddPropertySetter), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void GetPropertyInfo({}Property prop, bool&out isDisabled, bool&out isVirtual, bool&out isDict, bool&out isArray, bool&out isStringLike, string&out enumName, bool&out isInt, bool&out isFloat, bool&out isBool, int&out baseSize, bool&out isSynced) const", name).c_str(), FO_SCRIPT_GENERIC(Game_GetPropertyInfo), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
    };

    const auto register_entity_abstract = [&](const char* name, const EntityTypeDesc& desc) {
        const string sub_name = strex("Abstract{}", name);
        register_base_entity(sub_name.c_str());
        register_entity_cast(sub_name.c_str(), "Entity");
        register_entity_cast(name, sub_name.c_str());
        register_entity_getset(sub_name.c_str(), name);

        if (desc.HasProtos) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(sub_name.c_str(), "hstring get_ProtoId() const", FO_SCRIPT_FUNC_THIS(Entity_ProtoId), FO_SCRIPT_FUNC_THIS_CONV));
        }
    };

    const auto register_entity_protos = [&](const char* name, const EntityTypeDesc& desc) {
        const string sub_name = strex("Proto{}", name);
        register_base_entity(sub_name.c_str());
        register_entity_cast(sub_name.c_str(), "Entity");
        register_entity_getset(sub_name.c_str(), name);

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(sub_name.c_str(), "hstring get_ProtoId() const", FO_SCRIPT_FUNC_THIS(Entity_ProtoId), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "hstring get_ProtoId() const", FO_SCRIPT_FUNC_THIS(Entity_ProtoId), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("{}@+ get_Proto() const", sub_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_Proto), FO_SCRIPT_FUNC_THIS_CONV));

        if (desc.HasAbstract) {
            register_entity_cast(sub_name.c_str(), strex("Abstract{}", name).c_str());
        }
        if (!desc.Exported) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@+ Get{}(hstring pid)", sub_name, sub_name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntity), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("bool Check{}(hstring pid)", sub_name).c_str(), FO_SCRIPT_GENERIC(Game_CheckProtoCustomEntity), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("array<{}@>@ Get{}s()", sub_name, sub_name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntities), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        }
    };

    const auto register_fixed_type = [&](const char* name) {
        register_base_entity(name);
        register_entity_getset(name, name);
        register_entity_props(name);

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "hstring get_ProtoId() const", FO_SCRIPT_FUNC_THIS(Entity_ProtoId), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@+ Get{}(hstring pid)", name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntity), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("bool Check{}(hstring pid)", name).c_str(), FO_SCRIPT_GENERIC(Game_CheckProtoCustomEntity), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("array<{}@>@ Get{}s()", name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntities), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("array<{}@>@ Get{}({}Property property, any propertyValue)", name, name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntitiesByProperty), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("array<{}@>@ Get{}s({}Property property, any propertyValue)", name, name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntitiesByProperty), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
    };

    const auto register_entity_statics = [&](const char* name, const EntityTypeDesc& desc) {
        const string sub_name = strex("Static{}", name);
        register_base_entity(sub_name.c_str());
        register_entity_cast(sub_name.c_str(), "Entity");
        register_entity_getset(sub_name.c_str(), name);

        FO_AS_VERIFY(as_engine->RegisterObjectMethod(sub_name.c_str(), "ident get_StaticId() const", FO_SCRIPT_FUNC_THIS(Entity_Id), FO_SCRIPT_FUNC_THIS_CONV));

        if (desc.HasAbstract) {
            register_entity_cast(sub_name.c_str(), strex("Abstract{}", name).c_str());
        }
        if (desc.HasProtos) {
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(sub_name.c_str(), "hstring get_ProtoId() const", FO_SCRIPT_FUNC_THIS(Entity_ProtoId), FO_SCRIPT_FUNC_THIS_CONV));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(sub_name.c_str(), strex("Proto{}@+ get_Proto() const", name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_Proto), FO_SCRIPT_FUNC_THIS_CONV));
        }
    };

    const auto register_entity = [&](const char* name, const EntityTypeDesc& desc) {
        if (desc.IsGlobal) {
            const string singleton_name = strex("{}Singleton", name);
            FO_AS_VERIFY(as_engine->RegisterObjectType(singleton_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
            register_entity_getset(singleton_name.c_str(), name);
            register_entity_props(name);
        }
        else {
            register_base_entity(name);
            register_entity_cast(name, "Entity");
            register_entity_getset(name, name);
            register_entity_props(name);

            FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "ident get_Id() const", FO_SCRIPT_FUNC_THIS(Entity_Id), FO_SCRIPT_FUNC_THIS_CONV));

            if (backend->HasEntityMngr() && !desc.Exported) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@+ Get{}(ident id)", name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetEntity), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void Destroy({}@+ {})", name, name, strex(name).lower()).c_str(), FO_SCRIPT_GENERIC(Game_DestroyOne), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void Destroy(array<{}@>@+ {}s)", name, name, strex(name).lower()).c_str(), FO_SCRIPT_GENERIC(Game_DestroyAll), FO_SCRIPT_GENERIC_CONV, make_nptr(const_name(name).get()).void_cast()));
            }

            if (desc.HasAbstract) {
                register_entity_abstract(name, desc);
            }
            if (desc.HasProtos) {
                register_entity_protos(name, desc);
            }
            if (desc.HasStatics) {
                register_entity_statics(name, desc);
            }
        }
    };

    register_base_entity("Entity");

    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        if (string_view(type_name) == "Game") {
            register_entity(type_name.c_str(), type_desc);
        }
    }

    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        if (string_view(type_name) != "Game") {
            register_entity(type_name.c_str(), type_desc);
        }
    }

    for (const auto& type_name : meta->GetFixedTypes() | std::views::keys) {
        register_fixed_type(type_name.c_str());
    }

    FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", "void SetConstGlobalVar(?&out target, ?&in value)", FO_SCRIPT_GENERIC(Game_SetConstGlobalVar), FO_SCRIPT_GENERIC_CONV));

    // Register properties
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        auto registrator = type_desc.PropRegistrator.as_ptr();
        const string_view type_name_str = type_name.as_str();
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name_str).str() : string(type_name_str);
        const string abstract_class_name = strex("Abstract{}", class_name).str();
        const string proto_class_name = strex("Proto{}", class_name).str();
        const string static_class_name = strex("Static{}", class_name).str();

        for (const auto& [name, prop] : registrator->GetComponents()) {
            {
                const auto component_type = strex("{}{}Component", type_name_str, name).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@ get_{}() const", component_type, name).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("bool get_Has{}() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_HasComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
            if (type_desc.HasAbstract) {
                const auto component_type = strex("Abstract{}{}Component", type_name_str, name).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(abstract_class_name.c_str(), strex("{}@ get_{}() const", component_type, name).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(abstract_class_name.c_str(), strex("bool get_Has{}() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_HasComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
            if (type_desc.HasProtos) {
                const auto component_type = strex("Proto{}{}Component", type_name_str, name).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(proto_class_name.c_str(), strex("{}@ get_{}() const", component_type, name).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(proto_class_name.c_str(), strex("bool get_Has{}() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_HasComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
            if (type_desc.HasStatics) {
                const auto component_type = strex("Static{}{}Component", type_name_str, name).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(static_class_name.c_str(), strex("{}@ get_{}() const", component_type, name).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(static_class_name.c_str(), strex("bool get_Has{}() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_HasComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
        }

        for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
            auto prop = registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
            FO_VERIFY_AND_THROW(prop, "Property lookup by index returned null");
            const string handle_str_storage = [&]() -> string {
                if (prop->IsArray() || prop->IsDict() || prop->IsBaseTypeRefType()) {
                    return prop->IsNullable() ? "@?" : "@";
                }
                if (prop->IsBaseTypeProtoReference()) {
                    return prop->IsNullable() ? "@?+" : "@+";
                }
                return "";
            }();
            const string_view handle_str = handle_str_storage;

            if (!prop->IsDisabled() && !prop->IsComponentItself()) {
                const auto decl_get = strex("{}{} get_{}() const", MakeScriptPropertyName(prop.as_ptr()), handle_str, prop->GetNameWithoutComponent()).str();
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("{}{}Component", type_name_str, prop->GetComponentName()).c_str() : class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));

                if (!prop->IsVirtual() || prop->IsNullGetterForProto()) {
                    if (type_desc.HasAbstract) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("Abstract{}{}Component", type_name_str, prop->GetComponentName()).c_str() : abstract_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                    }
                    if (type_desc.HasProtos) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("Proto{}{}Component", type_name_str, prop->GetComponentName()).c_str() : proto_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                    }
                    if (type_desc.HasStatics) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("Static{}{}Component", type_name_str, prop->GetComponentName()).c_str() : static_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
                    }
                }
            }

            if (!prop->IsDisabled() && !prop->IsComponentItself() && prop->IsMutable()) {
                const string_view set_handle_str = !handle_str.empty() && handle_str[0] == '@' ? (prop->IsNullable() ? string_view {"@?+"} : string_view {"@+"}) : handle_str;
                const auto decl_set = strex("void set_{}({}{})", prop->GetNameWithoutComponent(), MakeScriptPropertyName(prop.as_ptr()), set_handle_str).str();
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("{}{}Component", type_name_str, prop->GetComponentName()).c_str() : class_name.c_str(), decl_set.c_str(), FO_SCRIPT_GENERIC(Entity_SetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
        }
    }

    for (auto&& [type_name, type_desc] : meta->GetFixedTypes()) {
        auto registrator = type_desc.PropRegistrator.as_ptr();
        const string_view type_name_str = type_name.as_str();
        const string type_name_storage {type_name_str};

        for (const auto& [name, prop] : registrator->GetComponents()) {
            const auto component_type = strex("{}{}Component", type_name_str, name).str();
            FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(type_name_storage.c_str(), strex("{}@ get_{}() const", component_type, name).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(type_name_storage.c_str(), strex("bool get_Has{}() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_HasComponent), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
        }

        for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
            auto prop = registrator->GetPropertyByIndex(numeric_cast<int32_t>(i));
            FO_VERIFY_AND_THROW(prop, "Property lookup by index returned null");
            const string handle_str_storage = [&]() -> string {
                if (prop->IsArray() || prop->IsDict() || prop->IsBaseTypeRefType()) {
                    return prop->IsNullable() ? "@?" : "@";
                }
                if (prop->IsBaseTypeProtoReference()) {
                    return prop->IsNullable() ? "@?+" : "@+";
                }
                return "";
            }();
            const string_view handle_str = handle_str_storage;

            if (!prop->IsDisabled() && !prop->IsComponentItself()) {
                const auto decl_get = strex("{}{} get_{}() const", MakeScriptPropertyName(prop.as_ptr()), handle_str, prop->GetNameWithoutComponent()).str();
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(prop->IsInComponent() ? strex("{}{}Component", type_name_str, prop->GetComponentName()).c_str() : type_name_storage.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, make_nptr(prop.get()).void_cast()));
            }
        }
    }

    // Register methods
    unordered_set<string> registered_funcdefs;

    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name).str() : string(type_name.as_str());

        for (const auto& method : type_desc.Methods) {
            // Funcdef variants
            for (const auto& arg : method.Args) {
                if (arg.Type.Kind == ComplexTypeKind::Callback) {
                    string cb_args = strex(",").join(vec_transform(span(*arg.Type.CallbackArgs).subspan(1), [](auto&& t) -> string { return MakeScriptArgName(t); }));
                    const string funcdef = strex("{} {}({})", MakeScriptReturnName(arg.Type.CallbackArgs->front()), MakeScriptTypeName(arg.Type), cb_args);

                    if (registered_funcdefs.emplace(funcdef).second) {
                        FO_AS_VERIFY(as_engine->RegisterFuncdef(funcdef.c_str()));
                    }
                }
            }

            int32_t registered_id = -1;

            if (method.GlobalGetter) {
                FO_VERIFY_AND_THROW(type_name.as_str() == "Game", "Global property access is bound to a non-Game type");
                FO_VERIFY_AND_THROW(method.Getter, "Method getter is not set");
                FO_VERIFY_AND_THROW(!method.Setter, "Method setter is already set");
                FO_VERIFY_AND_THROW(method.Args.empty(), "Method arguments must be empty before this operation");

                const string getter_decl = strex("{} get_{}()", MakeScriptReturnName(method.Ret, method.PassOwnership, method.ReturnNullable), method.Name);
                registered_id = as_engine->RegisterGlobalFunction(getter_decl.c_str(), FO_SCRIPT_GENERIC(Entity_GlobalMethodCall), FO_SCRIPT_GENERIC_CONV, make_nptr(&method).void_cast());
                FO_AS_VERIFY(registered_id);
            }
            else {
                const string possible_getset = strex("{}", method.Getter ? "get_" : (method.Setter ? "set_" : ""));
                const string method_decl = strex("{} {}{}({})", MakeScriptReturnName(method.Ret, method.PassOwnership, method.ReturnNullable), possible_getset, method.Name, MakeScriptArgsName(method.Args));
                registered_id = as_engine->RegisterObjectMethod(class_name.c_str(), method_decl.c_str(), FO_SCRIPT_GENERIC(Entity_MethodCall), FO_SCRIPT_GENERIC_CONV, make_nptr(&method).void_cast());
                FO_AS_VERIFY(registered_id);
            }

            if (method.Async) {
                SetFunctionAttributes(as_engine->GetFunctionById(registered_id), {"Async"});
            }
        }
    }

    // Register events
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name).str() : string(type_name.as_str());

        for (const auto& event : type_desc.Events) {
            const string first_arg = !type_desc.IsGlobal ? strex("{}@+ self", type_name, strex(type_name).lower()) : string();
            const string event_args_decl = MakeScriptArgsName(event.Args);
            const string event_obj_args_decl = strex("{}{}{}", first_arg, !first_arg.empty() && !event_args_decl.empty() ? ", " : "", event_args_decl);
            const string event_funcdef_void = strex("{}{}EventFunc", type_name, event.Name);
            const string event_funcdef_result = strex("{}{}EventFuncResult", type_name, event.Name);
            const string event_funcdef_void_decl = strex("void {}({})", event_funcdef_void, event_obj_args_decl);
            const string event_funcdef_result_decl = strex("EventResult {}({})", event_funcdef_result, event_obj_args_decl);
            const string event_type_name = strex("{}{}Event", type_name, event.Name);

            FO_AS_VERIFY(as_engine->RegisterFuncdef(event_funcdef_void_decl.c_str()));
            FO_AS_VERIFY(as_engine->RegisterFuncdef(event_funcdef_result_decl.c_str()));
            FO_AS_VERIFY(as_engine->RegisterObjectType(event_type_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Subscribe({}@+ func, EventPriority priority = EventPriority::Normal)", event_funcdef_void).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Subscribe), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Subscribe({}@+ func, EventPriority priority = EventPriority::Normal)", event_funcdef_result).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Subscribe), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Unsubscribe({}@+ func)", event_funcdef_void).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Unsubscribe), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Unsubscribe({}@+ func)", event_funcdef_result).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Unsubscribe), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), "void UnsubscribeAll()", FO_SCRIPT_GENERIC(EntityEvent_UnsubscribeAll), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@ get_{}()", event_type_name, event.Name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetSelfForEvent), FO_SCRIPT_FUNC_THIS_CONV));

            if (!event.Exported) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("EventResult Fire({})", event_args_decl).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Fire), FO_SCRIPT_GENERIC_CONV, make_nptr(&event).void_cast()));
            }
        }
    }

    // Register entity holders
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name).str() : string(type_name.as_str());

        for (const auto& holder_entry : type_desc.HolderEntries) {
            auto entry_name = make_ptr(&holder_entry.first);
            auto entry_type = make_ptr(&holder_entry.second.TargetType);

            if (backend->HasEntityMngr()) {
                if (type_desc.HasProtos) {
                    FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Add{}(hstring pid)", *entry_type, *entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_Add), FO_SCRIPT_GENERIC_CONV, make_nptr(entry_name.get()).void_cast()));
                }
                else {
                    FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Add{}()", *entry_type, *entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_Add), FO_SCRIPT_GENERIC_CONV, make_nptr(entry_name.get()).void_cast()));
                }
            }

            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("bool Has{}s() const", *entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_HasAny), FO_SCRIPT_GENERIC_CONV, make_nptr(entry_name.get()).void_cast()));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("array<{}@>@ Get{}s()", *entry_type, *entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_GetAll), FO_SCRIPT_GENERIC_CONV, make_nptr(entry_name.get()).void_cast()));

            if (type_name.as_str() != "Game") {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Get{}(ident id)", *entry_type, *entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_GetOne), FO_SCRIPT_GENERIC_CONV, make_nptr(entry_name.get()).void_cast()));
            }
        }
    }
}

FO_END_NAMESPACE

#endif
