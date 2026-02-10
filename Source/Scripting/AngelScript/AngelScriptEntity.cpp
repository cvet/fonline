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
#include "AngelScriptBackend.h"
#include "AngelScriptCall.h"
#include "AngelScriptContext.h"
#include "AngelScriptHelpers.h"
#include "EngineBase.h"
#include "Entity.h"

FO_BEGIN_NAMESPACE

static void Entity_AddRef(const Entity* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    self->AddRef();
}

static void Entity_Release(const Entity* self)
{
    FO_NO_STACK_TRACE_ENTRY();

    self->Release();
}

static auto Entity_IsDestroyed(const Entity* self) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on destroyed entity
    return self->IsDestroyed();
}

static auto Entity_IsDestroying(const Entity* self) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    // May call on destroyed entity
    return self->IsDestroying();
}

static auto Entity_Name(const Entity* self) -> string
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(self);
    return string(self->GetName());
}

static auto Entity_Id(const Entity* self) -> ident_t
{
    FO_STACK_TRACE_ENTRY();

    // May call on destroyed entity
    return self->GetId();
}

static auto Entity_ProtoId(const Entity* self) -> hstring
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(self);

    if (const auto* self_proto = dynamic_cast<const ProtoEntity*>(self)) {
        return self_proto->GetProtoId();
    }
    if (const auto* self_with_proto = dynamic_cast<const EntityWithProto*>(self)) {
        return self_with_proto->GetProtoId();
    }

    return {};
}

static auto Entity_Proto(const Entity* self) -> const Entity*
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(self);

    if (const auto* self_proto = dynamic_cast<const ProtoEntity*>(self)) {
        return self_proto;
    }
    if (const auto* self_with_proto = dynamic_cast<const EntityWithProto*>(self)) {
        return self_with_proto->GetProto();
    }

    return {};
}

static auto Entity_GetSelfForEvent(Entity* entity) -> Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    // Don't verify entity for destroying
    return entity;
}

static auto Entity_GetValueAsInt(const Entity* entity, int32 prop_index) -> int32
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(entity);

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);

    if (prop == nullptr) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    return entity->GetValueAsInt(prop);
}

static void Entity_SetValueAsInt(Entity* entity, int32 prop_index, int32 value)
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(entity);

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);

    if (prop == nullptr) {
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

static auto Entity_GetValueAsAny(const Entity* entity, int32 prop_index) -> any_t
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(entity);

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);

    if (prop == nullptr) {
        throw ScriptException("Property invalid enum", prop_index);
    }

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    return entity->GetValueAsAny(prop);
}

static void Entity_SetValueAsAny(Entity* entity, int32 prop_index, any_t value)
{
    FO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(entity);

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);

    if (prop == nullptr) {
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
    FO_NO_STACK_TRACE_ENTRY();

    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto& component = *cast_from_void<const hstring*>(gen->GetAuxiliary());
    bool has_component = false;
    CheckScriptEntityNonDestroyed(entity);

    if (const auto* proto_entity = dynamic_cast<const ProtoEntity*>(entity); proto_entity != nullptr) {
        has_component = proto_entity->HasComponent(component);
    }
    else if (const auto* entity_with_proto = dynamic_cast<const EntityWithProto*>(entity); entity_with_proto != nullptr) {
        has_component = entity_with_proto->GetProto()->HasComponent(component);
    }

    if (has_component) {
        new (gen->GetAddressOfReturnLocation()) Entity*(entity);
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
    }
}

static void Entity_GetPropertyValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto* prop = cast_from_void<const Property*>(gen->GetAuxiliary());
    const auto& getter = prop->GetGetter();
    CheckScriptEntityNonNull(entity);
    CheckScriptEntityNonDestroyed(entity);

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        if (!getter) {
            throw ScriptException("Getter not set");
        }

        prop_data = getter(entity, prop);
    }
    else {
        const auto& props = entity->GetProperties();

        props.ValidateForRawData(prop);

        const auto prop_raw_data = props.GetRawData(prop);

        prop_data.Pass(prop_raw_data);
    }

    ConvertPropsToScriptObject(prop, prop_data, gen->GetAddressOfReturnLocation(), gen->GetEngine());
}

static void Entity_SetPropertyValue(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto* prop = cast_from_void<const Property*>(gen->GetAuxiliary());
    auto& props = entity->GetPropertiesForEdit();
    void* as_obj = gen->GetAddressOfArg(0);
    CheckScriptEntityNonNull(entity);
    CheckScriptEntityNonDestroyed(entity);

    auto prop_data = ConvertScriptToPropsObject(prop, as_obj);
    props.SetValue(prop, prop_data);
}

static auto Entity_DownCast(Entity* entity) -> Entity*
{
    FO_NO_STACK_TRACE_ENTRY();

    CheckScriptEntityNonDestroyed(entity);
    return entity;
}

static void Entity_UpCast(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto entity_type_name = entity->GetTypeName();
    const auto& target_type_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    bool valid_cast = false;
    CheckScriptEntityNonDestroyed(entity);

    if (entity != nullptr && target_type_name.find(entity_type_name) != string::npos) {
        if (target_type_name.starts_with("Proto")) {
            valid_cast = dynamic_cast<ProtoEntity*>(entity) != nullptr;
        }
        else if (target_type_name.starts_with("Static")) {
            valid_cast = !entity->GetId();
        }
        else if (target_type_name.starts_with("Abstract")) {
            valid_cast = true;
        }
        else {
            FO_RUNTIME_ASSERT(target_type_name == entity_type_name.as_str());
            valid_cast = dynamic_cast<ProtoEntity*>(entity) == nullptr && !!entity->GetId();
        }
    }

    if (valid_cast) {
        new (gen->GetAddressOfReturnLocation()) Entity*(entity);
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
    }
}

static void Game_GetProtoCustomEntity(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto* engine = GetGameEngine(gen->GetEngine());
    const auto& pid = *cast_from_void<hstring*>(gen->GetAddressOfArg(0));
    const auto entity_hname = engine->Hashes.ToHashedString(entity_name);
    const auto* proto = engine->ProtoMngr.GetProtoEntitySafe(entity_hname, pid);

    if (proto != nullptr) {
        const auto* casted_proto = dynamic_cast<const ProtoCustomEntity*>(proto);
        FO_RUNTIME_ASSERT(casted_proto);
        new (gen->GetAddressOfReturnLocation()) Entity*(const_cast<ProtoCustomEntity*>(casted_proto));
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
    }
}

static void Game_GetProtoCustomEntities(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto* engine = GetGameEngine(gen->GetEngine());
    const auto entity_type = engine->Hashes.ToHashedString(entity_name);
    const auto& protos = engine->ProtoMngr.GetProtoEntities(entity_type);

    auto* as_engine = gen->GetEngine();
    auto* result = CreateScriptArray(as_engine, strex("{}[]", entity_type).c_str());
    result->Reserve(numeric_cast<int32>(protos.size()));

    for (const auto& proto : protos | std::views::values) {
        void* ptr = proto.get_no_const();
        result->InsertLast(static_cast<void*>(&ptr));
    }

    new (gen->GetAddressOfReturnLocation()) ScriptArray*(result);
}

static void Game_GetEntity(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    auto* backend = GetScriptBackend(gen->GetEngine());
    const auto& id = *cast_from_void<ident_t*>(gen->GetAddressOfArg(1));
    const auto entity_hname = backend->GetMetadata()->Hashes.ToHashedString(entity_name);
    auto* entity_mngr = backend->GetEntityMngr();

    auto* entity = entity_mngr->GetCustomEntity(entity_hname, id);
    new (gen->GetAddressOfReturnLocation()) Entity*(entity);
}

static void Game_DestroyOne(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* backend = GetScriptBackend(gen->GetEngine());
    auto* entity_mngr = backend->GetEntityMngr();
    auto* entity = *cast_from_void<Entity**>(gen->GetAddressOfArg(0));

    if (entity != nullptr) {
        entity_mngr->DestroyEntity(entity);
    }
}

static void Game_DestroyAll(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    auto* backend = GetScriptBackend(gen->GetEngine());
    auto* entity_mngr = backend->GetEntityMngr();
    const auto* entities = *cast_from_void<ScriptArray**>(gen->GetAddressOfArg(0));

    for (int32 i = 0; i < entities->GetSize(); i++) {
        auto* entity = *cast_from_void<Entity**>(entities->At(i));

        if (entity != nullptr) {
            entity_mngr->DestroyEntity(entity);
        }
    }
}

static void CustomEntity_Add(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entry = *cast_from_void<const hstring*>(gen->GetAuxiliary());
    auto* holder = cast_from_void<Entity*>(gen->GetObject());
    const auto pid = gen->GetArgCount() == 1 ? *cast_from_void<hstring*>(gen->GetAddressOfArg(0)) : hstring();
    auto* backend = GetScriptBackend(gen->GetEngine());
    auto* entity_mngr = backend->GetEntityMngr();

    auto* entity = entity_mngr->CreateCustomInnerEntity(holder, entry, pid);
    new (gen->GetAddressOfReturnLocation()) Entity*(entity);
}

static void CustomEntity_HasAny(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entry = *cast_from_void<const hstring*>(gen->GetAuxiliary());
    const auto* holder = cast_from_void<Entity*>(gen->GetObject());
    const auto* entities = holder->GetInnerEntities(entry);

    new (gen->GetAddressOfReturnLocation()) bool(entities != nullptr);
}

static void CustomEntity_GetOne(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entry = *cast_from_void<const hstring*>(gen->GetAuxiliary());
    auto* holder = cast_from_void<Entity*>(gen->GetObject());
    const auto id = *cast_from_void<ident_t*>(gen->GetAddressOfArg(0));
    auto* entities = holder->GetInnerEntities(entry);

    if (entities != nullptr && !entities->empty()) {
        const auto it = std::ranges::find_if(*entities, [id](auto&& entity) {
            CheckScriptEntityNonDestroyed(entity.get());
            return entity->GetId() == id;
        });

        if (it != entities->end()) {
            new (gen->GetAddressOfReturnLocation()) Entity*(it->get());
        }
        else {
            new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
        }
    }
    else {
        new (gen->GetAddressOfReturnLocation()) Entity*(nullptr);
    }
}

static void CustomEntity_GetAll(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entry = *cast_from_void<const hstring*>(gen->GetAuxiliary());
    auto* holder = cast_from_void<Entity*>(gen->GetObject());
    auto* entities = holder->GetInnerEntities(entry);
    auto* as_engine = gen->GetEngine();
    const auto* meta = GetEngineMetadata(as_engine);
    const auto& holder_type = meta->GetEntityType(holder->GetTypeName());
    const auto& holder_entry = holder_type.HolderEntries.at(entry);

    if (entities != nullptr && !entities->empty()) {
        vector<Entity*> result_entities;
        result_entities.reserve(entities->size());

        for (auto& entity : *entities) {
            CheckScriptEntityNonDestroyed(entity.get());
            result_entities.emplace_back(entity.get());
        }

        auto* arr = CreateScriptArray(as_engine, strex("{}[]", holder_entry.TargetType).c_str());
        arr->Reserve(numeric_cast<int32>(result_entities.size()));

        for (auto* result_entity : result_entities) {
            arr->InsertLast(cast_to_void(&result_entity));
        }

        new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr);
    }
    else {
        auto* arr = CreateScriptArray(as_engine, strex("{}[]", holder_entry.TargetType).c_str());
        new (gen->GetAddressOfReturnLocation()) ScriptArray*(arr);
    }
}

static void Game_SetPropertyGetter(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    auto* as_engine = gen->GetEngine();
    auto* backend = GetScriptBackend(as_engine);
    const auto* registrator = backend->GetMetadata()->GetPropertyRegistrator(entity_name);
    const auto prop_enum = *cast_from_void<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));

    if (static_cast<int32>(prop_enum) == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    const auto* prop = registrator->GetPropertyByIndex(static_cast<int32>(prop_enum));
    FO_RUNTIME_ASSERT(prop);

    if (const auto* as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); as_type_info == nullptr || as_type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(gen->GetArgAddress(1));

    if (!prop->IsVirtual()) {
        throw ScriptException("Property is not virtual", prop->GetName());
    }
    if (func->GetParamCount() != 1 && func->GetParamCount() != 2) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() == AngelScript::asTYPEID_VOID) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    if (MakeScriptPropertyName(prop) != strex(as_engine->GetTypeDeclaration(func->GetReturnTypeId())).replace("[]@", "[]").str()) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    int32 type_id;
    AngelScript::asDWORD flags;
    int32 as_result;
    FO_AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (const auto* as_type_info = as_engine->GetTypeInfoById(type_id); as_type_info == nullptr || string_view(as_type_info->GetName()) != entity_name || flags != 0) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    if (func->GetParamCount() == 2) {
        FO_AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (type_id != gen->GetArgTypeId(0) || flags != 0) {
            throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
        }
    }

    prop->SetGetter([=](Entity* entity, const Property*) -> PropertyRawData FO_DEFERRED {
        CheckScriptEntityNonDestroyed(entity);

        auto* context_mngr = backend->GetContextMngr();
        auto* ctx = context_mngr->PrepareContext(func);
        auto return_ctx = scope_exit([&]() noexcept { context_mngr->ReturnContext(ctx); });
        ctx->SetArgObject(0, entity); // May be null for protos

        if (func->GetParamCount() == 2) {
            ctx->SetArgDWord(1, prop->GetRegIndex());
        }

        const auto run_ok = context_mngr->RunContext(ctx, false);
        FO_RUNTIME_ASSERT(run_ok);

        auto prop_data = ConvertScriptToPropsObject(prop, ctx->GetAddressOfReturnValue());
        prop_data.StoreIfPassed();
        return prop_data;
    });
}

static void Game_AddPropertySetter(AngelScript::asIScriptGeneric* gen, bool deferred)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    auto* as_engine = gen->GetEngine();
    auto* backend = GetScriptBackend(as_engine);
    const auto* registrator = backend->GetMetadata()->GetPropertyRegistrator(entity_name);
    const auto prop_enum = *cast_from_void<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));

    if (static_cast<int32>(prop_enum) == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    const auto* prop = registrator->GetPropertyByIndex(static_cast<int32>(prop_enum));
    FO_RUNTIME_ASSERT(prop);

    if (const auto* as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); as_type_info == nullptr || as_type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(gen->GetArgAddress(1));

    if (func->GetParamCount() != 1 && func->GetParamCount() != 2 && func->GetParamCount() != 3) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() != AngelScript::asTYPEID_VOID) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    int32 type_id;
    AngelScript::asDWORD flags;
    FO_AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (const auto* as_type_info = as_engine->GetTypeInfoById(type_id); as_type_info == nullptr || string_view(as_type_info->GetName()) != entity_name || flags != 0) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    bool has_proto_enum = false;
    bool has_value_ref = false;

    if (func->GetParamCount() > 1) {
        FO_AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (MakeScriptPropertyName(prop) == strex(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == AngelScript::asTM_INOUTREF) {
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

            if (MakeScriptPropertyName(prop) == strex(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == AngelScript::asTM_INOUTREF) {
                has_value_ref = true;
            }
            else {
                throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
            }
        }
    }

    if (deferred && has_value_ref) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    prop->AddSetter([=, &as_result](Entity* entity, const Property*, PropertyRawData& prop_data) FO_DEFERRED {
        CheckScriptEntityNonDestroyed(entity);

        auto* context_mngr = backend->GetContextMngr();
        auto* ctx = context_mngr->PrepareContext(func);
        auto return_ctx = scope_exit([&]() noexcept { context_mngr->ReturnContext(ctx); });
        context_mngr->AddSetupScriptContextEntity(ctx, entity);

        FO_AS_VERIFY(ctx->SetArgObject(0, entity));

        if (has_proto_enum) {
            FO_AS_VERIFY(ctx->SetArgDWord(1, prop->GetRegIndex()));
        }

        if (!deferred) {
            if (has_value_ref) {
                PropertyRawData value_ref_space;
                auto* construct_addr = value_ref_space.Alloc(CalcConstructAddrSpace(prop));
                ConvertPropsToScriptObject(prop, prop_data, construct_addr, as_engine);
                FO_AS_VERIFY(ctx->SetArgAddress(has_proto_enum ? 2 : 1, construct_addr));

                const auto run_ok = context_mngr->RunContext(ctx, false);
                FO_RUNTIME_ASSERT(run_ok);

                prop_data = ConvertScriptToPropsObject(prop, construct_addr);
                prop_data.StoreIfPassed();
                FreeConstructAddrSpace(prop, construct_addr);
            }
            else {
                context_mngr->RunContext(ctx, true);
            }
        }
        else {
            // Will be run from the scheduler
            ignore_unused(has_value_ref);
            ignore_unused(prop_data);
            return_ctx.reset();
        }
    });
}

static void Game_AddPropertySetter_Deferred(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    Game_AddPropertySetter(gen, true);
}

static void Game_AddPropertySetter_NonDeferred(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    Game_AddPropertySetter(gen, false);
}

static void Game_GetPropertyInfo(AngelScript::asIScriptGeneric* gen)
{
    FO_STACK_TRACE_ENTRY();

    const auto* engine = GetGameEngine(gen->GetEngine());
    const auto prop_enum = static_cast<int32>(gen->GetArgDWord(0));
    bool& is_virtual = *cast_from_void<bool*>(gen->GetArgAddress(1));
    bool& is_dict = *cast_from_void<bool*>(gen->GetArgAddress(2));
    bool& is_array = *cast_from_void<bool*>(gen->GetArgAddress(3));
    bool& is_string_like = *cast_from_void<bool*>(gen->GetArgAddress(4));
    string& enum_name = *cast_from_void<string*>(gen->GetArgAddress(5));
    bool& is_int = *cast_from_void<bool*>(gen->GetArgAddress(6));
    bool& is_float = *cast_from_void<bool*>(gen->GetArgAddress(7));
    bool& is_bool = *cast_from_void<bool*>(gen->GetArgAddress(8));
    int32& base_size = *cast_from_void<int32*>(gen->GetArgAddress(9));
    bool& is_synced = *cast_from_void<bool*>(gen->GetArgAddress(10));

    if (prop_enum == 0) {
        throw ScriptException("'None' is not valid property entry in this context");
    }

    const auto& entity_name = *cast_from_void<const string*>(gen->GetAuxiliary());
    const auto* registrator = engine->GetPropertyRegistrator(entity_name);
    const auto* prop = registrator->GetPropertyByIndex(prop_enum);
    FO_RUNTIME_ASSERT(prop);

    is_virtual = prop->IsVirtual();
    is_dict = prop->IsDict();
    is_array = prop->IsArray();
    is_string_like = prop->IsBaseTypeString() || prop->IsBaseTypeHash();
    enum_name = prop->IsBaseTypeEnum() ? prop->GetBaseTypeName() : "";
    is_int = prop->IsBaseTypeInt();
    is_float = prop->IsBaseTypeFloat();
    is_bool = prop->IsBaseTypeBool();
    base_size = numeric_cast<int32>(prop->GetBaseSize());
    is_synced = prop->IsSynced();
}

static void Entity_MethodCall(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& method = *cast_from_void<const MethodDesc*>(gen->GetAuxiliary());
    FO_RUNTIME_ASSERT(method.Call);

    ScriptGenericCall(gen, true, [&](FuncCallData& call) { method.Call(call); });
}

static void EntityEvent_Subscribe(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& event = *cast_from_void<const EntityEventDesc*>(gen->GetAuxiliary());
    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(gen->GetAddressOfArg(0));
    FO_RUNTIME_ASSERT(func->GetReturnTypeId() == AngelScript::asTYPEID_VOID || func->GetReturnTypeId() == AngelScript::asTYPEID_BOOL);
    const auto& priority = *cast_from_void<Entity::EventPriority*>(gen->GetAddressOfArg(1));

    const auto* func_desc = IndexScriptFunc(func);
    FO_RUNTIME_ASSERT(func_desc->Call);

    Entity::EventCallbackData event_data;

    event_data.Callback = [func_ = refcount_ptr(func)](FuncCallData& call) mutable -> bool FO_DEFERRED {
        const bool event_has_result = func_->GetReturnTypeId() == AngelScript::asTYPEID_BOOL;
        bool event_result = false;
        call.RetData = event_has_result ? cast_to_void(&event_result) : nullptr;
        ScriptFuncCall(func_.get(), call);
        const bool result = !event_has_result || event_result;
        return result;
    };

    event_data.SubscriptionPtr = std::bit_cast<uintptr_t>(func);
    event_data.ExPolicy = Entity::EventExceptionPolicy::IgnoreAndContinueChain;
    event_data.Priority = priority;
    event_data.OneShot = false;
    event_data.Deferred = false;

    entity->SubscribeEvent(event.Name, std::move(event_data));
}

static void EntityEvent_Unsubscribe(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& event = *cast_from_void<const EntityEventDesc*>(gen->GetAuxiliary());
    auto* entity = cast_from_void<Entity*>(gen->GetObject());
    const auto* func = *cast_from_void<AngelScript::asIScriptFunction**>(gen->GetAddressOfArg(0));

    entity->UnsubscribeEvent(event.Name, std::bit_cast<uintptr_t>(func));
}

static void EntityEvent_UnsubscribeAll(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& event = *cast_from_void<const EntityEventDesc*>(gen->GetAuxiliary());
    auto* entity = cast_from_void<Entity*>(gen->GetObject());

    entity->UnsubscribeAllEvent(event.Name);
}

static void EntityEvent_Fire(AngelScript::asIScriptGeneric* gen)
{
    FO_NO_STACK_TRACE_ENTRY();

    const auto& event = *cast_from_void<const EntityEventDesc*>(gen->GetAuxiliary());
    auto* entity = cast_from_void<Entity*>(gen->GetObject());

    if (entity->HasEventCallbacks(event.Name)) {
        ScriptGenericCall(gen, !entity->IsGlobal(), [&](FuncCallData& call) {
            const bool result = entity->FireEvent(event.Name, call);
            new (gen->GetAddressOfReturnLocation()) bool(result);
        });
    }
    else {
        new (gen->GetAddressOfReturnLocation()) bool(true);
    }
}

void RegisterAngelScriptEntity(AngelScript::asIScriptEngine* as_engine)
{
    FO_STACK_TRACE_ENTRY();

    int32 as_result = 0;
    auto* backend = GetScriptBackend(as_engine);
    const auto* meta = backend->GetMetadata();

    // Register entities
    const auto const_name = [&](const char* name) -> const string* {
        const auto hname = meta->Hashes.ToHashedString(name);
        return &hname.as_str();
    };

    const auto register_base_entity = [&](const char* name) {
        FO_AS_VERIFY(as_engine->RegisterObjectType(name, 0, AngelScript::asOBJ_REF));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_ADDREF, "void f()", FO_SCRIPT_FUNC_THIS(Entity_AddRef), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectBehaviour(name, AngelScript::asBEHAVE_RELEASE, "void f()", FO_SCRIPT_FUNC_THIS(Entity_Release), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "bool get_IsDestroyed() const", FO_SCRIPT_FUNC_THIS(Entity_IsDestroyed), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "bool get_IsDestroying() const", FO_SCRIPT_FUNC_THIS(Entity_IsDestroying), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, "string get_Name() const", FO_SCRIPT_FUNC_THIS(Entity_Name), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_cast = [&](const char* name, const char* base_name) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(base_name, strex("{}@+ opCast()", name).c_str(), FO_SCRIPT_GENERIC(Entity_UpCast), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(base_name, strex("const {}@+ opCast() const", name).c_str(), FO_SCRIPT_GENERIC(Entity_UpCast), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("{}@+ opImplCast()", base_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_DownCast), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("const {}@+ opImplCast() const", base_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_DownCast), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_getset = [&](const char* name, const char* prop_name) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("int GetAsInt({}Property prop) const", prop_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetValueAsInt), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("void SetAsInt({}Property prop, int value)", prop_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_SetValueAsInt), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("any GetAsAny({}Property prop) const", prop_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetValueAsAny), FO_SCRIPT_FUNC_THIS_CONV));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod(name, strex("void SetAsAny({}Property prop, any value)", prop_name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_SetValueAsAny), FO_SCRIPT_FUNC_THIS_CONV));
    };

    const auto register_entity_props = [&](const char* name) {
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void SetPropertyGetter({}Property prop, ?&in func)", name).c_str(), FO_SCRIPT_GENERIC(Game_SetPropertyGetter), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void AddPropertySetter({}Property prop, ?&in func)", name).c_str(), FO_SCRIPT_GENERIC(Game_AddPropertySetter_NonDeferred), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void AddPropertyDeferredSetter({}Property prop, ?&in func)", name).c_str(), FO_SCRIPT_GENERIC(Game_AddPropertySetter_Deferred), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void GetPropertyInfo({}Property prop, bool&out isVirtual, bool&out isDict, bool&out isArray, bool&out isStringLike, string&out enumName, bool&out isInt, bool&out isFloat, bool&out isBool, int&out baseSize, bool&out isSynced) const", name).c_str(), FO_SCRIPT_GENERIC(Game_GetPropertyInfo), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
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
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@+ Get{}(hstring pid)", sub_name, sub_name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntity), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@[]@ Get{}s()", sub_name, sub_name).c_str(), FO_SCRIPT_GENERIC(Game_GetProtoCustomEntities), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
        }
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
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("{}@+ Get{}(ident id)", name, name).c_str(), FO_SCRIPT_GENERIC(Game_GetEntity), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void Destroy({}@+ {})", name, name, strex(name).lower()).c_str(), FO_SCRIPT_GENERIC(Game_DestroyOne), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod("GameSingleton", strex("void Destroy({}@[]@+ {}s)", name, name, strex(name).lower()).c_str(), FO_SCRIPT_GENERIC(Game_DestroyAll), FO_SCRIPT_GENERIC_CONV, cast_to_void(const_name(name))));
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

    // Register properties
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const auto& registrator = type_desc.PropRegistrator;
        const auto& type_name_str = type_name.as_str();
        const auto class_name = type_desc.IsGlobal ? type_name_str + "Singleton" : type_name_str;
        const auto abstract_class_name = "Abstract" + class_name;
        const auto proto_class_name = "Proto" + class_name;
        const auto static_class_name = "Static" + class_name;

        for (const auto& component : registrator->GetComponents()) {
            {
                const auto component_type = strex("{}{}Component", type_name_str, component).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, cast_to_void(&component)));
            }
            if (type_desc.HasAbstract) {
                const auto component_type = strex("Abstract{}{}Component", type_name_str, component).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(abstract_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, cast_to_void(&component)));
            }
            if (type_desc.HasProtos) {
                const auto component_type = strex("Proto{}{}Component", type_name_str, component).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(proto_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, cast_to_void(&component)));
            }
            if (type_desc.HasStatics) {
                const auto component_type = strex("Static{}{}Component", type_name_str, component).str();
                FO_AS_VERIFY(as_engine->RegisterObjectType(component_type.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(static_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), FO_SCRIPT_GENERIC(Entity_GetComponent), FO_SCRIPT_GENERIC_CONV, cast_to_void(&component)));
            }
        }

        for (size_t i = 1; i < registrator->GetPropertiesCount(); i++) {
            const auto* prop = registrator->GetPropertyByIndex(numeric_cast<int32>(i));
            const auto component = prop->GetComponent();
            const auto is_handle = prop->IsArray() || prop->IsDict();

            if (!prop->IsDisabled()) {
                const auto decl_get = strex("{}{} get_{}() const", MakeScriptPropertyName(prop), is_handle ? "@" : "", prop->GetNameWithoutComponent()).str();
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(component ? strex("{}{}Component", type_name_str, component).c_str() : class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop)));

                if (!prop->IsVirtual() || prop->IsNullGetterForProto()) {
                    if (type_desc.HasAbstract) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(component ? strex("Abstract{}{}Component", type_name_str, component).c_str() : abstract_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop)));
                    }
                    if (type_desc.HasProtos) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(component ? strex("Proto{}{}Component", type_name_str, component).c_str() : proto_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop)));
                    }
                    if (type_desc.HasStatics) {
                        FO_AS_VERIFY(as_engine->RegisterObjectMethod(component ? strex("Static{}{}Component", type_name_str, component).c_str() : static_class_name.c_str(), decl_get.c_str(), FO_SCRIPT_GENERIC(Entity_GetPropertyValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop)));
                    }
                }
            }

            if (!prop->IsDisabled() && prop->IsMutable()) {
                const auto decl_set = strex("void set_{}({}{})", prop->GetNameWithoutComponent(), MakeScriptPropertyName(prop), is_handle ? "@+" : "").str();
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(component ? strex("{}{}Component", type_name_str, component).c_str() : class_name.c_str(), decl_set.c_str(), FO_SCRIPT_GENERIC(Entity_SetPropertyValue), FO_SCRIPT_GENERIC_CONV, cast_to_void(prop)));
            }
        }
    }

    // Register methods
    unordered_set<string> registered_funcdefs;

    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name) : type_name.as_str();

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

            const string possible_getset = strex("{}", method.Getter ? "get_" : (method.Setter ? "set_" : ""));
            const string method_decl = strex("{} {}{}({})", MakeScriptReturnName(method.Ret, method.PassOwnership), possible_getset, method.Name, MakeScriptArgsName(method.Args));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), method_decl.c_str(), FO_SCRIPT_GENERIC(Entity_MethodCall), FO_SCRIPT_GENERIC_CONV, cast_to_void(&method)))
        }
    }

    // Register events
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name) : type_name.as_str();

        for (const auto& event : type_desc.Events) {
            const string first_arg = !type_desc.IsGlobal ? strex("{}@+ self", type_name, strex(type_name).lower()) : string();
            const string event_args_decl = MakeScriptArgsName(event.Args);
            const string event_obj_args_decl = strex("{}{}{}", first_arg, !first_arg.empty() && !event_args_decl.empty() ? ", " : "", event_args_decl);
            const string event_funcdef_void = strex("{}{}EventFunc", type_name, event.Name);
            const string event_funcdef_bool = strex("{}{}EventFuncBool", type_name, event.Name);
            const string event_funcdef_void_decl = strex("void {}({})", event_funcdef_void, event_obj_args_decl);
            const string event_funcdef_bool_decl = strex("bool {}({})", event_funcdef_bool, event_obj_args_decl);
            const string event_type_name = strex("{}{}Event", type_name, event.Name);

            FO_AS_VERIFY(as_engine->RegisterFuncdef(event_funcdef_void_decl.c_str()));
            FO_AS_VERIFY(as_engine->RegisterFuncdef(event_funcdef_bool_decl.c_str()));
            FO_AS_VERIFY(as_engine->RegisterObjectType(event_type_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Subscribe({}@+ func, EventPriority priority = EventPriority::Normal)", event_funcdef_void).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Subscribe), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Subscribe({}@+ func, EventPriority priority = EventPriority::Normal)", event_funcdef_bool).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Subscribe), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Unsubscribe({}@+ func)", event_funcdef_void).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Unsubscribe), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("void Unsubscribe({}@+ func)", event_funcdef_bool).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Unsubscribe), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), "void UnsubscribeAll()", FO_SCRIPT_GENERIC(EntityEvent_UnsubscribeAll), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@ get_{}()", event_type_name, event.Name).c_str(), FO_SCRIPT_FUNC_THIS(Entity_GetSelfForEvent), FO_SCRIPT_FUNC_THIS_CONV));

            if (!event.Exported) {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(event_type_name.c_str(), strex("bool Fire({})", event_args_decl).c_str(), FO_SCRIPT_GENERIC(EntityEvent_Fire), FO_SCRIPT_GENERIC_CONV, cast_to_void(&event)));
            }
        }
    }

    // Register entity holders
    for (auto&& [type_name, type_desc] : meta->GetEntityTypes()) {
        const string class_name = type_desc.IsGlobal ? strex("{}Singleton", type_name) : type_name.as_str();

        for (const auto& holder_entry : type_desc.HolderEntries) {
            const auto& entry_name = holder_entry.first;
            const auto& entry_type = holder_entry.second.TargetType;

            if (backend->HasEntityMngr()) {
                if (type_desc.HasProtos) {
                    FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Add{}(hstring pid)", entry_type, entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_Add), FO_SCRIPT_GENERIC_CONV, cast_to_void(&entry_name)));
                }
                else {
                    FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Add{}()", entry_type, entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_Add), FO_SCRIPT_GENERIC_CONV, cast_to_void(&entry_name)));
                }
            }

            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("bool Has{}s() const", entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_HasAny), FO_SCRIPT_GENERIC_CONV, cast_to_void(&entry_name)));
            FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@[]@ Get{}s()", entry_type, entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_GetAll), FO_SCRIPT_GENERIC_CONV, cast_to_void(&entry_name)));

            if (type_name.as_str() != "Game") {
                FO_AS_VERIFY(as_engine->RegisterObjectMethod(class_name.c_str(), strex("{}@+ Get{}(ident id)", entry_type, entry_name).c_str(), FO_SCRIPT_GENERIC(CustomEntity_GetOne), FO_SCRIPT_GENERIC_CONV, cast_to_void(&entry_name)));
            }
        }
    }
}

FO_END_NAMESPACE

#endif
