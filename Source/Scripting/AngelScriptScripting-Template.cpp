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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// ReSharper disable CppInconsistentNaming
// ReSharper disable CppCStyleCast
// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppClangTidyReadabilityQualifiedAuto
// ReSharper disable CppClangTidyModernizeUseNodiscard
// ReSharper disable CppClangTidyClangDiagnosticExtraSemiStmt
// ReSharper disable CppUseAuto
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppClangTidyClangDiagnosticOldStyleCast

///@ CodeGen Template AngelScript

///@ CodeGen Defines

#include "Common.h"

#if !COMPILER_MODE
#if SERVER_SCRIPTING
#include "Server.h"
#include "ServerScripting.h"
#elif CLIENT_SCRIPTING
#include "Client.h"
#include "ClientScripting.h"
#elif SINGLE_SCRIPTING
#include "Single.h"
#include "SingleScripting.h"
#elif MAPPER_SCRIPTING
#include "Mapper.h"
#include "MapperScripting.h"
#endif
#endif

#include "Application.h"
#include "DiskFileSystem.h"
#include "EngineBase.h"
#include "Entity.h"
#include "EntityProperties.h"
#include "EntityProtos.h"
#include "FileSystem.h"
#include "Log.h"
#include "Properties.h"
#include "ScriptSystem.h"
#include "StringUtils.h"

#include "AngelScriptExtensions.h"
#include "AngelScriptReflection.h"
#include "AngelScriptScriptDict.h"

#include "../autowrapper/aswrappedcall.h"
#include "angelscript.h"
#include "datetime/datetime.h"
#include "preprocessor.h"
#include "scriptany/scriptany.h"
#include "scriptarray.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"

#if SERVER_SCRIPTING
#define BaseEntity ServerEntity
#elif CLIENT_SCRIPTING
#define BaseEntity ClientEntity
#elif SINGLE_SCRIPTING
#define BaseEntity ServerEntity
#elif MAPPER_SCRIPTING
#define BaseEntity ClientEntity
#endif

#if !COMPILER_MODE
#if SERVER_SCRIPTING
#define FOEngine FOServer
#define SCRIPTING_CLASS ServerScriptSystem
#define StorageData StorageData_Server
#define ASGlobal ASGlobal_Server
#elif CLIENT_SCRIPTING
#define FOEngine FOClient
#define SCRIPTING_CLASS ClientScriptSystem
#define StorageData StorageData_Client
#define ASGlobal ASGlobal_Client
#elif SINGLE_SCRIPTING
#define FOEngine FOSingle
#define SCRIPTING_CLASS SingleScriptSystem
#define StorageData StorageData_Single
#define ASGlobal ASGlobal_Single
#elif MAPPER_SCRIPTING
#define FOEngine FOMapper
#define SCRIPTING_CLASS MapperScriptSystem
#define StorageData StorageData_Mapper
#define ASGlobal ASGlobal_Mapper
#endif
#else
#if !COMPILER_VALIDATION_MODE
#if SERVER_SCRIPTING
#define FOEngine AngelScriptServerCompiler
#define SCRIPTING_CLASS ASCompiler_ServerScriptSystem
#define StorageData StorageData_ServerCompiler
#define ASGlobal ASGlobal_ServerCompiler
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompiler
#define SCRIPTING_CLASS ASCompiler_ClientScriptSystem
#define StorageData StorageData_ClientCompiler
#define ASGlobal ASGlobal_ClientCompiler
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompiler
#define SCRIPTING_CLASS ASCompiler_SingleScriptSystem
#define StorageData StorageData_SingleCompiler
#define ASGlobal ASGlobal_SingleCompiler
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompiler
#define SCRIPTING_CLASS ASCompiler_MapperScriptSystem
#define StorageData StorageData_MapperCompiler
#define ASGlobal ASGlobal_MapperCompiler
#endif
#else
#if SERVER_SCRIPTING
#define FOEngine AngelScriptServerCompilerValidation
#define SCRIPTING_CLASS ASCompiler_ServerScriptSystem_Validation
#define StorageData StorageData_ServerCompilerValidation
#define ASGlobal ASGlobal_ServerCompilerValidation
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompilerValidation
#define SCRIPTING_CLASS ASCompiler_ClientScriptSystem_Validation
#define StorageData StorageData_ClientCompilerValidation
#define ASGlobal ASGlobal_ClientCompilerValidation
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompilerValidation
#define SCRIPTING_CLASS ASCompiler_SingleScriptSystem_Validation
#define StorageData StorageData_SingleCompilerValidation
#define ASGlobal ASGlobal_SingleCompilerValidation
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompilerValidation
#define SCRIPTING_CLASS ASCompiler_MapperScriptSystem_Validation
#define StorageData StorageData_MapperCompilerValidation
#define ASGlobal ASGlobal_MapperCompilerValidation
#endif
#endif
#endif

#if COMPILER_MODE
DECLARE_EXCEPTION(ScriptCompilerException);

struct FOServer;
struct FOClient;
struct FOSingle;
struct FOMapper;

struct BaseEntity : Entity
{
};

#if COMPILER_VALIDATION_MODE
#define INIT_ARGS FOEngineBase** out_engine
#else
#define INIT_ARGS string_view script_path
#endif

struct SCRIPTING_CLASS : public ScriptSystem
{
    void InitAngelScriptScripting(INIT_ARGS);
};

#define ENTITY_VERIFY(e)

class FOEngine : public FOEngineBase
{
public:
#if SERVER_SCRIPTING
    FOEngine() :
        FOEngineBase(Dummy, PropertiesRelationType::ServerRelative, [this] {
            extern void AngelScript_ServerCompiler_RegisterData(FOEngineBase*);
            AngelScript_ServerCompiler_RegisterData(this);
            return nullptr;
        })
#elif CLIENT_SCRIPTING
    FOEngine() :
        FOEngineBase(Dummy, PropertiesRelationType::ClientRelative, [this] {
            extern void AngelScript_ClientCompiler_RegisterData(FOEngineBase*);
            AngelScript_ClientCompiler_RegisterData(this);
            return nullptr;
        })
#elif SINGLE_SCRIPTING
    FOEngine() :
        FOEngineBase(Dummy, PropertiesRelationType::BothRelative, [this] {
            extern void AngelScript_SingleCompiler_RegisterData(FOEngineBase*);
            AngelScript_SingleCompiler_RegisterData(this);
            return nullptr;
        })
#elif MAPPER_SCRIPTING
    FOEngine() :
        FOEngineBase(Dummy, PropertiesRelationType::BothRelative, [this] {
            extern void AngelScript_MapperCompiler_RegisterData(FOEngineBase*);
            AngelScript_MapperCompiler_RegisterData(this);
            return nullptr;
        })
#endif
    {
    }

    GlobalSettings Dummy {};
};
#endif

#if !COMPILER_MODE
#define INIT_ARGS

#define GET_AS_ENGINE_FROM_SELF() self->GetEngine()->ScriptSys->AngelScriptData->Engine
#define GET_SCRIPT_SYS_FROM_SELF() self->GetEngine()->ScriptSys->AngelScriptData.get()
#define GET_SCRIPT_SYS_FROM_AS_ENGINE(as_engine) static_cast<FOEngine*>(as_engine->GetUserData())->ScriptSys->AngelScriptData.get()
#define GET_GAME_ENGINE_FROM_AS_ENGINE(as_engine) static_cast<FOEngine*>(as_engine->GetUserData())

#define ENTITY_VERIFY(e) \
    if ((e) != nullptr && (e)->IsDestroyed()) { \
        throw ScriptException("Access to destroyed entity"); \
    }
#endif

#define AS_VERIFY(expr) \
    as_result = expr; \
    if (as_result < 0) { \
        throw ScriptInitException(#expr); \
    }

#ifdef AS_MAX_PORTABILITY
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#define SCRIPT_FUNC_FUNCTOR(type, name) WRAP_MFN(type, name)
#define SCRIPT_FUNC_FUNCTOR_CONV asCALL_GENERIC
#else
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#define SCRIPT_FUNC_FUNCTOR(type, name) asMETHOD(type, name)
#define SCRIPT_FUNC_FUNCTOR_CONV asCALL_THISCALL_ASGLOBAL
#endif

#if !COMPILER_MODE
struct StorageData;

static void CallbackException(asIScriptContext* ctx, void* param)
{
    // string str = ctx->GetExceptionString();
    // if (str != "Pass")
    //    HandleException(ctx, _str("Script exception: {}{}", str, !_str(str).endsWith('.') ? "." : ""));
}

struct ScriptSystem::AngelScriptImpl
{
    struct ContextData
    {
        string Info {};
        asIScriptContext* Parent {};
    };

    void CreateContext()
    {
        asIScriptContext* ctx = Engine->CreateContext();
        RUNTIME_ASSERT(ctx);

        int r = ctx->SetExceptionCallback(asFUNCTION(CallbackException), nullptr, asCALL_CDECL);
        RUNTIME_ASSERT(r >= 0);

        auto* ctx_data = new ContextData();
        ctx->SetUserData(ctx_data);

        FreeContexts.push_back(ctx);
    }

    void FinishContext(asIScriptContext* ctx)
    {
        auto it = std::find(FreeContexts.begin(), FreeContexts.end(), ctx);
        RUNTIME_ASSERT(it != FreeContexts.end());
        FreeContexts.erase(it);

        delete static_cast<ContextData*>(ctx->GetUserData());
        ctx->Release();
    }

    auto RequestContext() -> asIScriptContext*
    {
        if (FreeContexts.empty()) {
            CreateContext();
        }

        auto* ctx = FreeContexts.back();
        FreeContexts.pop_back();
        BusyContexts.push_back(ctx);
        return ctx;
    }

    void ReturnContext(asIScriptContext* ctx)
    {
        ctx->Unprepare();

        auto it = std::find(BusyContexts.begin(), BusyContexts.end(), ctx);
        RUNTIME_ASSERT(it != BusyContexts.end());
        BusyContexts.erase(it);
        FreeContexts.push_back(ctx);

        auto* ctx_data = static_cast<ContextData*>(ctx->GetUserData());
        ctx_data->Parent = nullptr;
        ctx_data->Info.clear();
    }

    auto PrepareContext(asIScriptFunction* func) -> asIScriptContext*
    {
        RUNTIME_ASSERT(func);

        auto* ctx = RequestContext();

        const auto as_result = ctx->Prepare(func);
        if (as_result < 0) {
            throw ScriptException("Can't prepare context", func->GetName(), as_result);
        }

        return ctx;
    }

    auto RunContext(asIScriptContext* ctx, bool can_suspend) -> bool
    {
        auto* ctx_data = static_cast<ContextData*>(ctx->GetUserData());

        ctx_data->Parent = asGetActiveContext();

        const auto exec_result = ctx->Execute();

        if (exec_result == asEXECUTION_SUSPENDED) {
            if (!can_suspend) {
                ctx->Abort();
                ReturnContext(ctx);
                throw ScriptException("Can't yield current routine");
            }

            return false;
        }

        if (exec_result != asEXECUTION_FINISHED) {
            if (exec_result == asEXECUTION_EXCEPTION) {
                const string ex_string = ctx->GetExceptionString();
                auto* ex_func = ctx->GetExceptionFunction();
                const string ex_func_name = ex_func != nullptr ? ex_func->GetName() : "(unknown function)";
                const auto ex_line = ctx->GetExceptionLineNumber();
                ctx->Abort();
                ReturnContext(ctx);
                throw ScriptException("Script execution exception", ex_string, ex_func_name, ex_line);
            }

            if (exec_result == asEXECUTION_ABORTED) {
                ReturnContext(ctx);
                throw ScriptException("Execution of script aborted");
            }

            ctx->Abort();
            ReturnContext(ctx);
            throw ScriptException("Context execution error", exec_result);
        }

        return true;
    }

    auto CallGenericFunc(GenericScriptFunc* gen, asIScriptFunction* func, initializer_list<void*> args, void* ret) -> bool
    {
        RUNTIME_ASSERT(gen);
        RUNTIME_ASSERT(func);
        RUNTIME_ASSERT(gen->ArgsType.size() == args.size());
        RUNTIME_ASSERT(gen->ArgsType.size() == func->GetParamCount());

        if (ret != nullptr) {
            RUNTIME_ASSERT(func->GetReturnTypeId() != asTYPEID_VOID);
            RUNTIME_ASSERT(gen->RetType != &typeid(void));
        }
        else {
            RUNTIME_ASSERT(func->GetReturnTypeId() == asTYPEID_VOID);
            RUNTIME_ASSERT(gen->RetType == &typeid(void));
        }

        auto* ctx = PrepareContext(func);

        for (asUINT i = 0; i < func->GetParamCount(); i++) {
            // Marshalling
            // ctx->GetA
        }

        if (RunContext(ctx, ret == nullptr)) {
            if (ret != nullptr) {
                // std::memcpy(ret, func->GetAddressOfReturnLocation(), )
            }

            ReturnContext(ctx);
            return true;
        }

        return false;
    }

    FOEngine* GameEngine {};
    asIScriptEngine* Engine {};
    StorageData* Storage {};
    unordered_map<string, std::any> SettingsStorage {};
    asIScriptContext* CurrentCtx {};
    vector<asIScriptContext*> FreeContexts {};
    vector<asIScriptContext*> BusyContexts {};
};
#endif

template<typename T>
static T* ScriptableObject_Factory()
{
    return new T();
}

#if !COMPILER_MODE
// Arrays stuff
[[maybe_unused]] static auto CreateASArray(asIScriptEngine* as_engine, const char* type) -> CScriptArray*
{
    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(type_info);
    auto* as_array = CScriptArray::Create(type_info);
    RUNTIME_ASSERT(as_array);
    return as_array;
}

template<typename T>
[[maybe_unused]] static void VerifyEntityArray(asIScriptEngine* as_engine, CScriptArray* as_array)
{
    UNUSED_VARIABLE(as_engine);

    for (const auto i : xrange(as_array->GetSize())) {
        UNUSED_VARIABLE(i); // ENTITY_VERIFY may expand to none
        ENTITY_VERIFY(*reinterpret_cast<T*>(as_array->At(i)));
    }
}

template<typename T>
[[maybe_unused]] static auto MarshalArray(asIScriptEngine* as_engine, CScriptArray* as_array) -> vector<T>
{
    UNUSED_VARIABLE(as_engine);

    if (as_array == nullptr || as_array->GetSize() == 0u) {
        return {};
    }

    vector<T> vec;
    vec.resize(as_array->GetSize());
    for (const auto i : xrange(as_array->GetSize())) {
        vec[i] = *reinterpret_cast<T*>(as_array->At(i));
    }

    return vec;
}

template<typename T>
[[maybe_unused]] static auto MarshalBackScalarArray(asIScriptEngine* as_engine, const char* type, const vector<T>& vec) -> CScriptArray*
{
    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        as_array->Resize(static_cast<asUINT>(vec.size()));
        for (const auto i : xrange(vec)) {
            *reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i))) = vec[i];
        }
    }

    return as_array;
}

template<typename T>
[[maybe_unused]] static auto MarshalBackRefArray(asIScriptEngine* as_engine, const char* type, const vector<T>& vec) -> CScriptArray*
{
    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        as_array->Resize(static_cast<asUINT>(vec.size()));
        for (const auto i : xrange(vec)) {
            *reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i))) = vec[i];
            if (vec[i]) {
                vec[i]->AddRef();
            }
        }
    }

    return as_array;
}

[[maybe_unused]] static auto CreateASDict(asIScriptEngine* as_engine, const char* type) -> CScriptDict*
{
    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(type_info);
    auto* as_dict = CScriptDict::Create(type_info);
    RUNTIME_ASSERT(as_dict);
    return as_dict;
}

template<typename T, typename U>
[[maybe_unused]] static auto MarshalDict(asIScriptEngine* as_engine, CScriptDict* as_dict) -> map<T, U>
{
    // Todo: MarshalDict
    UNUSED_VARIABLE(as_engine);

    if (as_dict == nullptr || as_dict->GetSize() == 0u) {
        return {};
    }

    map<T, U> map;
    for (const auto i : xrange(as_dict->GetSize())) {
        UNUSED_VARIABLE(i);
        // vec[i] = *reinterpret_cast<Type*>(as_array->At(i));
    }

    return map;
}

template<typename T, typename U>
[[maybe_unused]] static auto MarshalBackScalarDict(asIScriptEngine* as_engine, const char* type, const map<T, U>& map) -> CScriptDict*
{
    // Todo: MarshalBackScalarDict
    auto* as_dict = CreateASDict(as_engine, type);

    if (!map.empty()) {
        // as_array->Resize(static_cast<asUINT>(vec.size()));
        // for (const auto i : xrange(vec)) {
        //    auto* p = reinterpret_cast<T*>(as_array->At(static_cast<asUINT>(i)));
        //    *p = vec[i];
        //}
    }

    return as_dict;
}

[[maybe_unused]] static auto GetASObjectInfo(void* ptr, int type_id) -> string
{
    // Todo: GetASObjectInfo
    UNUSED_VARIABLE(ptr);
    UNUSED_VARIABLE(type_id);
    return "";
}
#endif

[[maybe_unused]] static auto GetASFuncName(const asIScriptFunction* func) -> string
{
    if (func == nullptr) {
        return "";
    }

    if (func->GetNamespace() == nullptr) {
        return _str("{}", func->GetName());
    }

    return _str("{}::{}", func->GetNamespace(), func->GetName());
}

template<typename T>
static void Entity_AddRef(const T* self)
{
#if !COMPILER_MODE
    self->AddRef();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Entity_Release(const T* self)
{
#if !COMPILER_MODE
    self->Release();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_IsDestroyed(const T* self) -> bool
{
#if !COMPILER_MODE
    // May call on destroyed entity
    return self->IsDestroyed();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_IsDestroying(const T* self) -> bool
{
#if !COMPILER_MODE
    // May call on destroyed entity
    return self->IsDestroying();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_Name(const T* self) -> string
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return string(self->GetName());
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_Id(const T* self) -> uint
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return self->GetId();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_ProtoId(const T* self) -> hstring
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return self->GetProtoId();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<>
auto Entity_ProtoId(const Entity* self) -> hstring
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    if (const auto* self_with_proto = dynamic_cast<const EntityWithProto*>(self)) {
        return self_with_proto->GetProtoId();
    }
    return {};
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_Proto(const T* self) -> const ProtoEntity*
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return self->GetProto();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

///@ CodeGen Global

template<typename T>
static auto Property_GetValueAsInt(const T* entity, int prop_index) -> int
{
#if !COMPILER_MODE
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (!prop->IsReadable()) {
        throw ScriptException("Property is not readable");
    }

    return entity->GetValueAsInt(prop);

#else
    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop_index);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_SetValueAsInt(T* entity, int prop_index, int value)
{
#if !COMPILER_MODE
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (!prop->IsWritable()) {
        throw ScriptException("Property is not writeble");
    }

    entity->SetValueAsInt(prop, value);

#else
    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop_index);
    UNUSED_VARIABLE(value);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Property_GetValueAsFloat(const T* entity, int prop_index) -> float
{
#if !COMPILER_MODE
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (!prop->IsReadable()) {
        throw ScriptException("Property is not readable");
    }

    return entity->GetValueAsFloat(prop);

#else
    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop_index);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_SetValueAsFloat(T* entity, int prop_index, float value)
{
#if !COMPILER_MODE
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (!prop->IsWritable()) {
        throw ScriptException("Property is not writeble");
    }

    entity->SetValueAsFloat(prop, value);

#else
    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop_index);
    UNUSED_VARIABLE(value);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_GetValue(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto* entity = static_cast<T*>(gen->GetObject());
    const auto* prop = static_cast<const Property*>(gen->GetAuxiliary());
    ENTITY_VERIFY(entity);

    uint data_size;
    const auto* data = entity->GetProperties().GetRawData(prop, data_size);

    if (prop->IsPlainData()) {
        RUNTIME_ASSERT(data_size);
        memcpy(gen->GetAddressOfReturnLocation(), data, data_size);
    }
    else if (prop->IsString()) {
        new (gen->GetAddressOfReturnLocation()) string(reinterpret_cast<const char*>(data), data_size);
    }
    else if (prop->IsArray()) {
        auto* arr = CreateASArray(gen->GetEngine(), prop->GetFullTypeName().c_str());

        if (prop->IsArrayOfString()) {
            if (data_size > 0u) {
                uint arr_size;
                std::memcpy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);

                arr->Resize(arr_size);

                for (uint i = 0; i < arr_size; i++) {
                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr->SetValue(i, &str);

                    data += str_size;
                }
            }
        }
        else {
            if (data_size > 0u) {
                arr->Resize(data_size / prop->GetBaseSize());

                std::memcpy(arr->At(0), data, data_size);
            }
        }

        *static_cast<CScriptArray**>(gen->GetAddressOfReturnLocation()) = arr;
    }
    else if (prop->IsDict()) {
        CScriptDict* dict = CreateASDict(gen->GetEngine(), prop->GetFullTypeName().c_str());

        if (data_size > 0u) {
            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += prop->GetDictKeySize();

                    uint arr_size;
                    std::memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(arr_size);

                    auto* arr = CreateASArray(gen->GetEngine(), _str("{}[]", prop->GetBaseTypeName()).c_str());

                    if (arr_size > 0u) {
                        if (prop->IsDictOfArrayOfString()) {
                            arr->Resize(arr_size);

                            for (uint i = 0; i < arr_size; i++) {
                                uint str_size;
                                std::memcpy(&str_size, data, sizeof(str_size));
                                data += sizeof(str_size);

                                string str(reinterpret_cast<const char*>(data), str_size);
                                arr->SetValue(i, &str);
                                data += str_size;
                            }
                        }
                        else {
                            arr->Resize(arr_size);

                            std::memcpy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            data += arr_size * prop->GetBaseSize();
                        }
                    }

                    dict->Set((void*)key, &arr);
                    arr->Release();
                }
            }
            else if (prop->IsDictOfString()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += prop->GetDictKeySize();

                    uint str_size;
                    std::memcpy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    dict->Set((void*)key, &str);

                    data += str_size;
                }
            }
            else {
                const auto whole_element_size = prop->GetDictKeySize() + prop->GetBaseSize();
                const auto dict_size = data_size / whole_element_size;

                for (uint i = 0; i < dict_size; i++) {
                    dict->Set((void*)(data + i * whole_element_size), (void*)(data + i * whole_element_size + prop->GetDictKeySize()));
                }
            }
        }

        *static_cast<CScriptDict**>(gen->GetAddressOfReturnLocation()) = dict;
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
#else
    UNUSED_VARIABLE(gen);
#endif
}

template<typename T>
static void Property_SetValue(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<T*>(gen->GetObject());
    const auto* prop = static_cast<const Property*>(gen->GetAuxiliary());
    ENTITY_VERIFY(entity);

    auto& props = entity->GetPropertiesForEdit();
    void* data = gen->GetAddressOfArg(0);

    if (prop->IsPlainData()) {
        props.SetRawData(prop, static_cast<const uchar*>(data), prop->GetBaseSize());
    }
    else if (prop->IsString()) {
        const auto& str = *static_cast<const string*>(data);
        props.SetRawData(prop, reinterpret_cast<const uchar*>(str.data()), static_cast<uint>(str.length()));
    }
    else if (prop->IsArray()) {
        const auto* arr = *static_cast<CScriptArray**>(data);

        if (prop->IsArrayOfString()) {
            const auto arr_size = (arr != nullptr ? arr->GetSize() : 0u);

            if (arr_size > 0u) {
                // Calculate size
                uint data_size = sizeof(uint);
                for (uint i = 0; i < arr_size; i++) {
                    const auto& str = *static_cast<const string*>(arr->At(i));
                    data_size += sizeof(uint) + static_cast<uint>(str.length());
                }

                // Make buffer
                auto* init_buf = new uchar[data_size];
                auto* buf = init_buf;

                std::memcpy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint);

                for (uint i = 0; i < arr_size; i++) {
                    const auto& str = *static_cast<const string*>(arr->At(i));

                    uint str_size = static_cast<uint>(str.length());
                    std::memcpy(buf, &str_size, sizeof(str_size));
                    buf += sizeof(str_size);

                    if (str_size > 0u) {
                        std::memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }

                props.SetRawData(prop, init_buf, data_size);
                delete[] init_buf;
            }
            else {
                props.SetRawData(prop, nullptr, 0u);
            }
        }
        else {
            const auto data_size = (arr != nullptr ? arr->GetSize() * prop->GetBaseSize() : 0u);

            if (data_size > 0u) {
                props.SetRawData(prop, static_cast<const uchar*>(arr->At(0)), data_size);
            }
            else {
                props.SetRawData(prop, nullptr, 0u);
            }
        }
    }
    else if (prop->IsDict()) {
        const auto* dict = *static_cast<CScriptDict**>(data);

        if (prop->IsDictOfArray()) {
            if (dict != nullptr && dict->GetSize() > 0u) {
                // Calculate size
                uint data_size = 0u;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (const auto& [key, value] : dict_map) {
                    const auto* arr = *static_cast<const CScriptArray**>(value);
                    uint arr_size = (arr != nullptr ? arr->GetSize() : 0u);
                    data_size += prop->GetDictKeySize() + sizeof(arr_size);

                    if (prop->IsDictOfArrayOfString()) {
                        for (uint i = 0; i < arr_size; i++) {
                            const auto& str = *static_cast<const string*>(arr->At(i));
                            data_size += sizeof(uint) + static_cast<uint>(str.length());
                        }
                    }
                    else {
                        data_size += arr_size * prop->GetBaseSize();
                    }
                }

                // Make buffer
                auto* init_buf = new uchar[data_size];
                auto* buf = init_buf;

                for (const auto& [key, value] : dict_map) {
                    const auto* arr = *static_cast<const CScriptArray**>(value);
                    std::memcpy(buf, key, prop->GetDictKeySize());
                    buf += prop->GetDictKeySize();

                    const uint arr_size = (arr != nullptr ? arr->GetSize() : 0u);
                    std::memcpy(buf, &arr_size, sizeof(uint));
                    buf += sizeof(arr_size);

                    if (arr_size > 0u) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint i = 0; i < arr_size; i++) {
                                const auto& str = *static_cast<const string*>(arr->At(i));
                                const auto str_size = static_cast<uint>(str.length());

                                std::memcpy(buf, &str_size, sizeof(uint));
                                buf += sizeof(str_size);

                                if (str_size > 0u) {
                                    std::memcpy(buf, str.c_str(), str_size);
                                    buf += arr_size;
                                }
                            }
                        }
                        else {
                            std::memcpy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                            buf += arr_size * prop->GetBaseSize();
                        }
                    }
                }

                props.SetRawData(prop, init_buf, data_size);
                delete[] init_buf;
            }
            else {
                props.SetRawData(prop, nullptr, 0u);
            }
        }
        else if (prop->IsDictOfString()) {
            if (dict != nullptr && dict->GetSize() > 0u) {
                // Calculate size
                uint data_size = 0u;

                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (const auto& [key, value] : dict_map) {
                    const auto& str = *static_cast<const string*>(value);
                    const auto str_size = static_cast<uint>(str.length());

                    data_size += prop->GetDictKeySize() + sizeof(str_size) + str_size;
                }

                // Make buffer
                uchar* init_buf = new uchar[data_size];
                uchar* buf = init_buf;

                for (const auto& [key, value] : dict_map) {
                    const auto& str = *static_cast<const string*>(value);
                    std::memcpy(buf, key, prop->GetDictKeySize());
                    buf += prop->GetDictKeySize();

                    const auto str_size = static_cast<uint>(str.length());
                    std::memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(str_size);

                    if (str_size > 0u) {
                        std::memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }

                props.SetRawData(prop, init_buf, data_size);
                delete[] init_buf;
            }
            else {
                props.SetRawData(prop, nullptr, 0u);
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeySize();
            const auto value_element_size = prop->GetBaseSize();
            const auto whole_element_size = key_element_size + value_element_size;
            const auto data_size = (dict != nullptr ? dict->GetSize() * whole_element_size : 0u);

            if (data_size > 0u) {
                auto* init_buf = new uchar[data_size];
                auto* buf = init_buf;

                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (const auto& [key, value] : dict_map) {
                    std::memcpy(buf, key, key_element_size);
                    buf += key_element_size;
                    std::memcpy(buf, value, value_element_size);
                    buf += value_element_size;
                }

                props.SetRawData(prop, init_buf, data_size);
                delete[] init_buf;
            }
            else {
                props.SetRawData(prop, nullptr, 0u);
            }
        }
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
#else
    UNUSED_VARIABLE(gen);
#endif
}

template<typename T>
static Entity* EntityDownCast(T* a)
{
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return static_cast<Entity*>(a);
}

template<typename T>
static T* EntityUpCast(Entity* a)
{
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return dynamic_cast<T*>(a);
}

static void HashedString_Construct(hstring* self)
{
    new (self) hstring();
}

static void HashedString_ConstructFromString(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto& str = *static_cast<const string*>(gen->GetArgObject(0));
    auto hstr = GET_GAME_ENGINE_FROM_AS_ENGINE(gen->GetEngine())->ToHashedString(str);
    new (gen->GetObject()) hstring(hstr);
#endif
}

static void HashedString_ConstructFromHash(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto& hash = *static_cast<const uint*>(gen->GetArgObject(0));
    auto hstr = GET_GAME_ENGINE_FROM_AS_ENGINE(gen->GetEngine())->ResolveHash(hash);
    new (gen->GetObject()) hstring(hstr);
#endif
}

static void HashedString_ConstructCopy(hstring* self, const hstring& other)
{
    new (self) hstring(other);
}

static void HashedString_Assign(hstring& self, const hstring& other)
{
    self = other;
}

static bool HashedString_Equals(const hstring& self, const hstring& other)
{
    return self == other;
}

static bool HashedString_EqualsString(const hstring& self, const string& other)
{
    return self.as_str() == other;
}

static string HashedString_StringCast(const hstring& self)
{
    return string(self.as_str());
}

static string HashedString_GetString(const hstring& self)
{
    return string(self.as_str());
}

static int HashedString_GetHash(const hstring& self)
{
    return self.as_int();
}

static uint HashedString_GetUHash(const hstring& self)
{
    return self.as_uint();
}

struct StorageData
{
    unordered_set<CScriptArray*> EnumArrays {};

    ///@ CodeGen Storage
};

static void CallbackMessage(const asSMessageInfo* msg, void* param)
{
    UNUSED_VARIABLE(param);

    const char* type = "error";
    if (msg->type == asMSGTYPE_WARNING) {
        type = "warning";
    }
    else if (msg->type == asMSGTYPE_INFORMATION) {
        type = "info";
    }

    const auto formatted_message = _str("{}({},{}): {} : {}", Preprocessor::ResolveOriginalFile(msg->row), Preprocessor::ResolveOriginalLine(msg->row), msg->col, type, msg->message).str();

#if COMPILER_MODE
    extern unordered_set<string> CompilerPassedMessages;
    if (CompilerPassedMessages.count(formatted_message) == 0u) {
        CompilerPassedMessages.insert(formatted_message);
    }
    else {
        return;
    }
#endif

    WriteLog("{}", formatted_message);
}

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
static void CompileRootModule(asIScriptEngine* engine, string_view script_path);
#else
static void RestoreRootModule(asIScriptEngine* engine, const_span<uchar> script_bin);
#endif

void SCRIPTING_CLASS::InitAngelScriptScripting(INIT_ARGS)
{
#if !COMPILER_MODE
    FOEngine* game_engine = _engine;
    game_engine->AddRef();
#else
    FOEngine* game_engine = new FOEngine();
#endif

#if COMPILER_MODE
    static int dummy = 0;
#endif

    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    RUNTIME_ASSERT(engine);

    engine->SetUserData(game_engine);

#if !COMPILER_MODE
    AngelScriptData = std::make_unique<AngelScriptImpl>();
    AngelScriptData->GameEngine = game_engine;
    AngelScriptData->Engine = engine;
#endif

#if COMPILER_MODE
    StorageData Storage;
    auto* storage = &Storage;
#else
    AngelScriptData->Storage = new StorageData();
    auto* storage = AngelScriptData->Storage;
#endif

    storage->Global.self = game_engine;

    int as_result;
    AS_VERIFY(engine->SetMessageCallback(asFUNCTION(CallbackMessage), nullptr, asCALL_CDECL));

    AS_VERIFY(engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_DISALLOW_EMPTY_LIST_ELEMENTS, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_PRIVATE_PROP_AS_PROTECTED, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, true));
    AS_VERIFY(engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true));
#if COMPILER_MODE
    AS_VERIFY(engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, false));
#endif

    RegisterScriptArray(engine, true);
    ScriptExtensions::RegisterScriptArrayExtensions(engine);
    RegisterStdString(engine);
    ScriptExtensions::RegisterScriptStdStringExtensions(engine);
    RegisterScriptAny(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDict(engine);
    ScriptExtensions::RegisterScriptDictExtensions(engine);
    RegisterScriptFile(engine);
    RegisterScriptFileSystem(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
    RegisterScriptWeakRef(engine);
    RegisterScriptReflection(engine);

    // Register enums
    for (const auto& [enum_name, enum_pairs] : game_engine->GetAllEnums()) {
        AS_VERIFY(engine->RegisterEnum(enum_name.c_str()));
        for (const auto& [key, value] : enum_pairs) {
            AS_VERIFY(engine->RegisterEnumValue(enum_name.c_str(), key.c_str(), value));
        }
    }

    // Register hstring
    AS_VERIFY(engine->RegisterObjectType("hstring", sizeof(hstring), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<hstring>()));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS(HashedString_Construct), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_ConstructCopy), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const string &in)", asFUNCTION(HashedString_ConstructFromString), asCALL_GENERIC, game_engine));
    // AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const int &in)", asFUNCTION(HashedString_ConstructFromHash), asCALL_GENERIC, game_engine));
    // AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const uint &in)", asFUNCTION(HashedString_ConstructFromHash), asCALL_GENERIC, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromHash(int h)", asFUNCTION(HashedString_ConstructFromHash), asCALL_GENERIC, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromHash(uint h)", asFUNCTION(HashedString_ConstructFromHash), asCALL_GENERIC, game_engine));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "hstring &opAssign(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_Assign), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const hstring &in) const", SCRIPT_FUNC_THIS(HashedString_Equals), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const string &in) const", SCRIPT_FUNC_THIS(HashedString_EqualsString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string opImplCast() const", SCRIPT_FUNC_THIS(HashedString_StringCast), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string get_str() const", SCRIPT_FUNC_THIS(HashedString_GetString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "int get_hash() const", SCRIPT_FUNC_THIS(HashedString_GetHash), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "uint get_uhash() const", SCRIPT_FUNC_THIS(HashedString_GetUHash), SCRIPT_FUNC_THIS_CONV));

    // Entity registrators
#define REGISTER_BASE_ENTITY(class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectType(class_name, 0, asOBJ_REF)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_FUNC_THIS((Entity_AddRef<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_FUNC_THIS((Entity_Release<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroyed() const", SCRIPT_FUNC_THIS((Entity_IsDestroyed<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroying() const", SCRIPT_FUNC_THIS((Entity_IsDestroying<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "string get_Name() const", SCRIPT_FUNC_THIS((Entity_Name<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterFuncdef("bool " class_name "Predicate(" class_name "@+)")); \
    AS_VERIFY(engine->RegisterFuncdef("void " class_name "Callback(" class_name "@+)")); \
    AS_VERIFY(engine->RegisterFuncdef("void " class_name "InitFunc(" class_name "@+, bool)"))

#define REGISTER_ENTITY_CAST(class_name, real_class, base_class_name) \
    AS_VERIFY(engine->RegisterObjectMethod(base_class_name, class_name "@+ opCast()", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(base_class_name, "const " class_name "@+ opCast() const", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, base_class_name "@+ opImplCast()", SCRIPT_FUNC_THIS((EntityDownCast<Entity>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "const " base_class_name "@+ opImplCast() const", SCRIPT_FUNC_THIS((EntityDownCast<Entity>)), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_GETSET_ENTITY(class_name, prop_class_name, real_class) \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsInt(" prop_class_name "Property) const", SCRIPT_FUNC_THIS((Property_GetValueAsInt<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsInt(" prop_class_name "Property, int)", SCRIPT_FUNC_THIS((Property_SetValueAsInt<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsFloat(" prop_class_name "Property) const", SCRIPT_FUNC_THIS((Property_GetValueAsFloat<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsFloat(" prop_class_name "Property, float)", SCRIPT_FUNC_THIS((Property_SetValueAsFloat<real_class>)), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_GLOBAL_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name "Singleton", real_class); \
    REGISTER_GETSET_ENTITY(class_name "Singleton", class_name, real_class); \
    entity_is_global.emplace(class_name); \
    entity_get_value_func_ptr.emplace(class_name "Singleton", asFUNCTION((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name "Singleton", asFUNCTION((Property_SetValue<real_class>)))

#define REGISTER_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name, real_class); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY(class_name, class_name, real_class); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "uint get_Id() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_value_func_ptr.emplace(class_name, asFUNCTION((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name, asFUNCTION((Property_SetValue<real_class>)))

#define REGISTER_ENTITY_ABSTACT(class_name, real_class) \
    REGISTER_BASE_ENTITY("Abstract" class_name, Entity); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY("Abstract" class_name, class_name, Entity); \
    AS_VERIFY(engine->RegisterObjectMethod("Abstract" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<Entity>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_value_func_ptr.emplace("Abstract" class_name, asFUNCTION((Property_GetValue<Entity>)))

#define REGISTER_ENTITY_PROTO(class_name, real_class, proto_real_class) \
    REGISTER_BASE_ENTITY(class_name "Proto", proto_real_class); \
    REGISTER_ENTITY_CAST(class_name "Proto", proto_real_class, "Entity"); \
    REGISTER_ENTITY_CAST(class_name "Proto", proto_real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY(class_name "Proto", class_name, proto_real_class); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name "Proto", "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<proto_real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, class_name "Proto@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_has_protos.emplace(class_name); \
    entity_get_value_func_ptr.emplace(class_name "Proto", asFUNCTION((Property_GetValue<proto_real_class>)))

#define REGISTER_ENTITY_STATICS(class_name, real_class) \
    REGISTER_BASE_ENTITY("Static" class_name, real_class); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Entity"); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY("Static" class_name, class_name, real_class); \
    if (entity_has_protos.count(class_name)) { \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, class_name "Proto@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    } \
    entity_has_statics.emplace(class_name); \
    entity_get_value_func_ptr.emplace("Static" class_name, asFUNCTION((Property_GetValue<real_class>)))

    REGISTER_BASE_ENTITY("Entity", Entity);

    unordered_set<string> entity_is_global;
    unordered_set<string> entity_has_protos;
    unordered_set<string> entity_has_statics;
    unordered_map<string, asSFuncPtr> entity_get_value_func_ptr;
    unordered_map<string, asSFuncPtr> entity_set_value_func_ptr;

    // Events
#define REGISTER_ENTITY_EVENT(entity_name, is_global_entity, event_name, as_args_ent, as_args, func_entry) \
    AS_VERIFY(engine->RegisterFuncdef("void " entity_name event_name "EventFunc(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterFuncdef("bool " entity_name event_name "EventFuncBool(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterObjectType(entity_name event_name "Event", 0, asOBJ_REF | asOBJ_NOHANDLE)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void UnsubscribeAll()", SCRIPT_FUNC_THIS(func_entry##_UnsubscribeAll), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectProperty(is_global_entity ? entity_name "Singleton" : entity_name, entity_name event_name "Event On" event_name, 0))

#define REGISTER_ENTITY_EXPORTED_EVENT(entity_name, is_global_entity, event_name, as_args_ent, as_args, func_entry) REGISTER_ENTITY_EVENT(entity_name, is_global_entity, event_name, as_args_ent, as_args, func_entry)

#define REGISTER_ENTITY_SCRIPT_EVENT(entity_name, is_global_entity, event_name, as_args_ent, as_args, func_entry) \
    REGISTER_ENTITY_EVENT(entity_name, is_global_entity, event_name, as_args_ent, as_args, func_entry); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "bool Fire(" as_args ")", SCRIPT_FUNC_THIS(func_entry##_Fire), SCRIPT_FUNC_THIS_CONV))

    // Settings
    AS_VERIFY(engine->RegisterObjectType("GlobalSettings", 0, asOBJ_REF | asOBJ_NOHANDLE));
    AS_VERIFY(engine->RegisterGlobalProperty("GlobalSettings Settings", game_engine));

#define REGISTER_GET_SETTING(setting_entry, get_decl) AS_VERIFY(engine->RegisterObjectMethod("GlobalSettings", get_decl, SCRIPT_FUNC_THIS(ASSetting_Get_##setting_entry), SCRIPT_FUNC_THIS_CONV))
#define REGISTER_SET_SETTING(setting_entry, set_decl) AS_VERIFY(engine->RegisterObjectMethod("GlobalSettings", set_decl, SCRIPT_FUNC_THIS(ASSetting_Set_##setting_entry), SCRIPT_FUNC_THIS_CONV))

    // Remote calls
#if SERVER_SCRIPTING || CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterObjectType("RemoteCaller", 0, asOBJ_REF | asOBJ_NOHANDLE));
#endif

    ///@ CodeGen Register

    // Register properties
    for (const auto& [reg_name, registrator] : game_engine->GetAllPropertyRegistrators()) {
        const auto is_global = entity_is_global.count(reg_name) != 0u;
        const auto is_has_protos = entity_has_protos.count(reg_name) != 0u;
        const auto is_has_statics = entity_has_statics.count(reg_name) != 0u;
        const auto class_name = is_global ? reg_name + "Singleton" : reg_name;
        const auto abstract_class_name = "Abstract" + class_name;
        const auto proto_class_name = class_name + "Proto";
        const auto static_class_name = "Static" + class_name;
        const auto get_value_func_ptr = entity_get_value_func_ptr.at(class_name);
        const auto get_abstract_value_func_ptr = !is_global && (is_has_protos || is_has_statics) ? entity_get_value_func_ptr.at(abstract_class_name) : asSFuncPtr();
        const auto get_proto_value_func_ptr = is_has_protos ? entity_get_value_func_ptr.at(proto_class_name) : asSFuncPtr();
        const auto get_static_value_func_ptr = is_has_statics ? entity_get_value_func_ptr.at(static_class_name) : asSFuncPtr();
        const auto set_value_func_ptr = entity_set_value_func_ptr.at(class_name);

        for (const auto i : xrange(registrator->GetCount())) {
            const auto* prop = registrator->GetByIndex(i);
            const auto is_handle = (prop->IsArray() || prop->IsDict());

            if (prop->IsReadable()) {
                const auto decl_get = _str("const {}{} get_{}() const", prop->GetFullTypeName(), is_handle ? "@" : "", prop->GetName()).str();
                AS_VERIFY(engine->RegisterObjectMethod(class_name.c_str(), decl_get.c_str(), get_value_func_ptr, asCALL_GENERIC, (void*)prop));

                if (!is_global && (is_has_protos || is_has_statics)) {
                    AS_VERIFY(engine->RegisterObjectMethod(abstract_class_name.c_str(), decl_get.c_str(), get_abstract_value_func_ptr, asCALL_GENERIC, (void*)prop));
                }
                if (is_has_protos) {
                    AS_VERIFY(engine->RegisterObjectMethod(proto_class_name.c_str(), decl_get.c_str(), get_proto_value_func_ptr, asCALL_GENERIC, (void*)prop));
                }
                if (is_has_statics) {
                    AS_VERIFY(engine->RegisterObjectMethod(static_class_name.c_str(), decl_get.c_str(), get_static_value_func_ptr, asCALL_GENERIC, (void*)prop));
                }
            }

            if (prop->IsWritable()) {
                const auto decl_set = _str("void set_{}({}{}{})", prop->GetName(), is_handle ? "const " : "", prop->GetFullTypeName(), is_handle ? "@+" : "").str();
                AS_VERIFY(engine->RegisterObjectMethod(class_name.c_str(), decl_set.c_str(), set_value_func_ptr, asCALL_GENERIC, (void*)prop));
            }
        }

        for (const auto& [group_name, properties] : registrator->GetPropertyGroups()) {
            vector<ScriptEnum_uint16> prop_enums;
            prop_enums.reserve(properties.size());
            for (const auto* prop : properties) {
                prop_enums.push_back(static_cast<ScriptEnum_uint16>(prop->GetRegIndex()));
            }

#if !COMPILER_MODE
            const auto it_enum = storage->EnumArrays.emplace(MarshalBackScalarArray(engine, _str("{}Property[]", registrator->GetClassName()).c_str(), prop_enums));
            RUNTIME_ASSERT(it_enum.second);
            AS_VERIFY(engine->RegisterGlobalProperty(_str("{}Property[]@ {}Property{}", registrator->GetClassName(), registrator->GetClassName(), group_name).c_str(), (void*)&(*it_enum.first)));
#else
            AS_VERIFY(engine->RegisterGlobalProperty(_str("{}Property[]@ {}Property{}", registrator->GetClassName(), registrator->GetClassName(), group_name).c_str(), &dummy));
#endif
        }
    }

#if !COMPILER_MODE
    AS_VERIFY(engine->RegisterGlobalProperty("GameSingleton@ Game", &_engine));
#else
    AS_VERIFY(engine->RegisterGlobalProperty("GameSingleton@ Game", &dummy));
#endif

#if SERVER_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ClientCall", 0));
#endif

#if CLIENT_SCRIPTING
#if !COMPILER_MODE
    AS_VERIFY(engine->RegisterGlobalProperty("RemoteCaller ServerCall", &_engine));
    AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", &_engine->CurMap));
    AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", &_engine->_curLocation));
#else
    AS_VERIFY(engine->RegisterGlobalProperty("RemoteCaller ServerCall", &dummy));
    AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", &dummy));
    AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", &dummy));
#endif
#endif

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
    CompileRootModule(engine, script_path);
    engine->ShutDownAndRelease();
#else
#if COMPILER_VALIDATION_MODE
    game_engine->FileSys.AddDataSource("AngelScript");
#else
    game_engine->FileSys.AddDataSource(_str(game_engine->Settings.ResourcesDir).combinePath("AngelScript"));
#endif
#if SERVER_SCRIPTING
    File script_file = game_engine->FileSys.ReadFile("ServerRootModule.fosb");
#elif CLIENT_SCRIPTING
    File script_file = game_engine->FileSys.ReadFile("ClientRootModule.fosb");
#elif SINGLE_SCRIPTING
    File script_file = game_engine->FileSys.ReadFile("SingleRootModule.fosb");
#elif MAPPER_SCRIPTING
    File script_file = game_engine->FileSys.ReadFile("MapperRootModule.fosb");
#endif
    RUNTIME_ASSERT(script_file);
    RestoreRootModule(engine, {script_file.GetBuf(), script_file.GetSize()});
#endif

#if !COMPILER_MODE || COMPILER_VALIDATION_MODE
    // Index all functions
    {
        RUNTIME_ASSERT(engine->GetModuleCount() == 1);
        auto* mod = engine->GetModuleByIndex(0);

        const auto as_type_to_type_info = [](int type_id, asDWORD flags) -> const std::type_info* {
            const auto is_ref = (flags & asTM_INOUTREF) != 0;
            if (is_ref) {
                return nullptr;
            }

            switch (flags) {
            case asTYPEID_VOID:
                return &typeid(void);
            case asTYPEID_BOOL:
                return &typeid(bool);
            case asTYPEID_INT8:
                return &typeid(char);
            case asTYPEID_INT16:
                return &typeid(short);
            case asTYPEID_INT32:
                return &typeid(int);
            case asTYPEID_INT64:
                return &typeid(int64);
            case asTYPEID_UINT8:
                return &typeid(uchar);
            case asTYPEID_UINT16:
                return &typeid(ushort);
            case asTYPEID_UINT32:
                return &typeid(uint);
            case asTYPEID_UINT64:
                return &typeid(uint64);
            case asTYPEID_FLOAT:
                return &typeid(float);
            case asTYPEID_DOUBLE:
                return &typeid(double);
            }

            if ((type_id & asTYPEID_OBJHANDLE) != 0 && (type_id & asTYPEID_APPOBJECT) != 0) {
            }

            return nullptr;
        };

        for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            auto* func = mod->GetFunctionByIndex(i);

            // Bind
            const auto func_name = GetASFuncName(func);
            const auto it = _funcMap.emplace(std::make_pair(func_name, GenericScriptFunc()));
            auto& gen_func = it->second;

            gen_func.Name = func_name;
            gen_func.Declaration = func->GetDeclaration(true, true, true);

#if !COMPILER_VALIDATION_MODE
            gen_func.Call = std::bind(&ScriptSystem::AngelScriptImpl::CallGenericFunc, AngelScriptData.get(), &gen_func, func, std::placeholders::_1, std::placeholders::_2);
#endif

            for (asUINT p = 0; p < func->GetParamCount(); p++) {
                int param_type_id;
                asDWORD param_flags = 0;
                AS_VERIFY(func->GetParam(p, &param_type_id, &param_flags));

                gen_func.ArgsType.emplace_back(as_type_to_type_info(param_type_id, param_flags));
            }

            asDWORD ret_flags = 0;
            int ret_type_id = func->GetReturnTypeId(&ret_flags);
            gen_func.RetType = as_type_to_type_info(ret_type_id, ret_flags);

            gen_func.CallSupported = (gen_func.RetType != nullptr && std::find(gen_func.ArgsType.begin(), gen_func.ArgsType.end(), nullptr) == gen_func.ArgsType.end());

            // Check for special module init function
            if (gen_func.ArgsType.empty() && gen_func.RetType == &typeid(void)) {
                RUNTIME_ASSERT(gen_func.CallSupported);
                const auto func_name_ex = _str(func->GetName());
                if (func_name_ex.compareIgnoreCase("ModuleInit") || func_name_ex.compareIgnoreCase("module_init")) {
                    _initFunc.push_back(&gen_func);
                }
            }
        }
    }
#endif

#if COMPILER_MODE && COMPILER_VALIDATION_MODE
    RUNTIME_ASSERT(out_engine);
    *out_engine = game_engine;
    game_engine->AddRef();
#endif

    game_engine->Release();
}

class BinaryStream : public asIBinaryStream
{
public:
    explicit BinaryStream(std::vector<asBYTE>& buf) : _binBuf {buf} { }

    void Write(const void* ptr, asUINT size) override
    {
        if (!ptr || size == 0u) {
            return;
        }

        _binBuf.resize(_binBuf.size() + size);
        std::memcpy(&_binBuf[_writePos], ptr, size);
        _writePos += size;
    }

    void Read(void* ptr, asUINT size) override
    {
        if (!ptr || size == 0u) {
            return;
        }

        std::memcpy(ptr, &_binBuf[_readPos], size);
        _readPos += size;
    }

    auto GetBuf() -> std::vector<asBYTE>& { return _binBuf; }

private:
    std::vector<asBYTE>& _binBuf;
    size_t _readPos {};
    size_t _writePos {};
};

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
static void CompileRootModule(asIScriptEngine* engine, string_view script_path)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        explicit ScriptLoader(vector<uchar>& root) : _rootScript {&root} { }

        auto LoadFile(const string& dir, const string& file_name, vector<char>& data, string& file_path) -> bool override
        {
            if (_rootScript) {
                data.resize(_rootScript->size());
                std::memcpy(data.data(), _rootScript->data(), _rootScript->size());
                _rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            _includeDeep++;

            file_path = file_name;
            data.resize(0);

            if (_includeDeep == 1) {
                auto script_file = DiskFileSystem::OpenFile(file_name, false);
                if (!script_file) {
                    return false;
                }
                if (script_file.GetSize() == 0) {
                    return true;
                }

                data.resize(script_file.GetSize());
                if (!script_file.Read(data.data(), script_file.GetSize())) {
                    return false;
                }

                return true;
            }

            return Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
        }

        void FileLoaded() override { _includeDeep--; }

    private:
        vector<uchar>* _rootScript {};
        int _includeDeep {0};
    };

    auto script_file = DiskFileSystem::OpenFile(script_path, false);
    if (!script_file) {
        throw ScriptCompilerException("Root script file not found", script_path);
    }
    if (script_file.GetSize() == 0) {
        throw ScriptCompilerException("Root script file is empty", script_path);
    }

    vector<uchar> script_data(script_file.GetSize());
    if (!script_file.Read(script_data.data(), script_file.GetSize())) {
        throw ScriptCompilerException("Can't read root script file", script_path);
    }

    Preprocessor::UndefAll();
#if SERVER_SCRIPTING
    Preprocessor::Define("__SERVER");
#elif CLIENT_SCRIPTING
    Preprocessor::Define("__CLIENT");
#elif SINGLE_SCRIPTING
    Preprocessor::Define("__SINGLE");
    Preprocessor::Define("__SERVER");
    Preprocessor::Define("__CLIENT");
#elif MAPPER_SCRIPTING
    Preprocessor::Define("__MAPPER");
#endif

    ScriptLoader loader {script_data};
    Preprocessor::StringOutStream result, errors;
    const auto errors_count = Preprocessor::Preprocess("", result, &errors, &loader);

    while (!errors.String.empty() && errors.String.back() == '\n') {
        errors.String.pop_back();
    }

    if (errors_count > 0) {
        throw ScriptCompilerException("Preprocessor failed", errors.String);
    }
    else if (!errors.String.empty()) {
        WriteLog("Preprocessor message: {}", errors.String);
    }

    asIScriptModule* mod = engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (mod == nullptr) {
        throw ScriptCompilerException("Create root module failed");
    }

    int as_result = mod->AddScriptSection("Root", result.String.c_str());
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to add script section", as_result);
    }

    as_result = mod->Build();
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to build module", as_result);
    }

    vector<asBYTE> buf;
    BinaryStream binary {buf};
    as_result = mod->SaveByteCode(&binary);
    if (as_result < 0) {
        throw ScriptCompilerException("Unable to save byte code", as_result);
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::GetLineNumberTranslator();
    vector<uchar> lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt, lnt_data);

    vector<uchar> data;
    auto writer = DataWriter(data);
    writer.Write<uint>(static_cast<uint>(buf.size()));
    writer.WritePtr(buf.data(), buf.size());
    writer.Write<uint>(static_cast<uint>(lnt_data.size()));
    writer.WritePtr(lnt_data.data(), lnt_data.size());

    const auto script_out_path = _str("AngelScript/{}b", _str(script_path).extractFileName()).str();
    auto file = DiskFileSystem::OpenFile(script_out_path, true);
    if (!file) {
        throw ScriptCompilerException("Can't write binary to file", script_out_path);
    }
    if (!file.Write(data)) {
        throw ScriptCompilerException("Can't write binary to file", script_out_path);
    }
}

#else
static void RestoreRootModule(asIScriptEngine* engine, const_span<uchar> script_bin)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);
    RUNTIME_ASSERT(!script_bin.empty());

    auto reader = DataReader({script_bin.data(), script_bin.size()});
    vector<asBYTE> buf(reader.Read<uint>());
    std::memcpy(buf.data(), reader.ReadPtr<asBYTE>(buf.size()), buf.size());
    vector<uchar> lnt_data(reader.Read<uint>());
    std::memcpy(lnt_data.data(), reader.ReadPtr<uchar>(lnt_data.size()), lnt_data.size());
    reader.VerifyEnd();
    RUNTIME_ASSERT(!buf.empty());
    RUNTIME_ASSERT(!lnt_data.empty());

    asIScriptModule* mod = engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (mod == nullptr) {
        throw ScriptException("Create root module fail");
    }

    Preprocessor::LineNumberTranslator* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    UNUSED_VARIABLE(lnt);
    mod->SetUserData(lnt);

    BinaryStream binary {buf};
    int as_result = mod->LoadByteCode(&binary);
    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }
}
#endif
