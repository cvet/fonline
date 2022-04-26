//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#elif MAPPER_SCRIPTING
#include "Mapper.h"
#include "MapperScripting.h"
#endif
#endif

#if SERVER_SCRIPTING
#define SCRIPTING_CLASS ServerScriptSystem
#define FOEngine FOServer
#define BaseEntity ServerEntity
#elif CLIENT_SCRIPTING
#define SCRIPTING_CLASS ClientScriptSystem
#define FOEngine FOClient
#define BaseEntity ClientEntity
#elif MAPPER_SCRIPTING
#define SCRIPTING_CLASS MapperScriptSystem
#define FOEngine FOMapper
#define BaseEntity ClientEntity
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

#if COMPILER_MODE
#undef FOEngine
#define FOEngine FOEngineBase

DECLARE_EXCEPTION(ScriptCompilerException);

struct FOServer;
struct FOClient;
struct FOMapper;

struct BaseEntity : Entity
{
};

#define INIT_ARGS const char* script_path
struct SCRIPTING_CLASS
{
    void InitAngelScriptScripting(INIT_ARGS);
};

#define ENTITY_VERIFY(e)

#if SERVER_SCRIPTING
class AngelScriptServerCompilerData : public FOEngineBase
#elif CLIENT_SCRIPTING
class AngelScriptClientCompilerData : public FOEngineBase
#elif MAPPER_SCRIPTING
class AngelScriptMapperCompilerData : public FOEngineBase
#endif
{
public:
#if SERVER_SCRIPTING
    AngelScriptServerCompilerData();
#elif CLIENT_SCRIPTING
    AngelScriptClientCompilerData();
#elif MAPPER_SCRIPTING
    AngelScriptMapperCompilerData();
#endif
};
#endif

#if !COMPILER_MODE
#define INIT_ARGS

#define GET_AS_ENGINE_FROM_SELF() self->GetEngine()->ScriptSys->AngelScriptData->Engine
#define GET_SCRIPT_SYS_FROM_SELF() self->GetEngine()->ScriptSys->AngelScriptData.get()
#define GET_SCRIPT_SYS_FROM_AS_ENGINE(as_engine) static_cast<FOEngine*>(as_engine->GetUserData())->ScriptSys->AngelScriptData.get()

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
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#define SCRIPT_FUNC_FUNCTOR_CONV asCALL_GENERIC
#define SCRIPT_METHOD_FUNCTOR_CONV asCALL_GENERIC
#else
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#define SCRIPT_FUNC_FUNCTOR_CONV asCALL_THISCALL_ASGLOBAL
#define SCRIPT_METHOD_FUNCTOR_CONV asCALL_THISCALL_OBJFIRST
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
                throw ScriptException("Can't yield current routine");
            }

            return false;
        }

        if (exec_result != asEXECUTION_FINISHED) {
            ctx->Abort();
            ReturnContext(ctx);

            if (exec_result == asEXECUTION_EXCEPTION) {
                throw ScriptException("Script execution exception", ctx->GetExceptionString(), ctx->GetExceptionFunction()->GetName(), ctx->GetExceptionLineNumber());
            }
            if (exec_result == asEXECUTION_ABORTED) {
                throw ScriptException("Execution of script aborted");
            }

            throw ScriptException("Context execution error", exec_result);
        }

        return true;
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

[[maybe_unused]] static auto GetASFuncName(asIScriptFunction* func) -> string
{
    if (func == nullptr) {
        return "";
    }

    return _str("AngelScript.{}", func->GetName());
}
#endif

static auto Entity_IsDestroyed(Entity* self) -> bool
{
#if !COMPILER_MODE
    // May call on destroyed entity
    return self->IsDestroyed();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Entity_IsDestroying(Entity* self) -> bool
{
#if !COMPILER_MODE
    // May call on destroyed entity
    return self->IsDestroying();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Entity_Name(Entity* self) -> string
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return string(self->GetName());
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Entity_Id(BaseEntity* self) -> uint
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return self->GetId();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Entity_ProtoId(BaseEntity* self) -> hstring
{
#if !COMPILER_MODE
    ENTITY_VERIFY(self);
    return self->GetProtoId();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Entity_Proto(BaseEntity* self) -> const ProtoEntity*
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

static void Property_GetValueAsInt(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<Entity*>(gen->GetObject());
    const auto prop_index = *static_cast<int*>(gen->GetAddressOfArg(0));
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

    new (gen->GetAddressOfReturnLocation()) int(entity->GetValueAsInt(prop));
#else
    UNUSED_VARIABLE(gen);
#endif
}

static void Property_SetValueAsInt(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<Entity*>(gen->GetObject());
    const auto prop_index = *static_cast<int*>(gen->GetAddressOfArg(0));
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

    entity->SetValueAsInt(prop, *static_cast<int*>(gen->GetAddressOfArg(1)));
#else
    UNUSED_VARIABLE(gen);
#endif
}

static void Property_GetValueAsFloat(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<Entity*>(gen->GetObject());
    const auto prop_index = *static_cast<int*>(gen->GetAddressOfArg(0));
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

    new (gen->GetAddressOfReturnLocation()) float(entity->GetValueAsFloat(prop));
#else
    UNUSED_VARIABLE(gen);
#endif
}

static void Property_SetValueAsFloat(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<Entity*>(gen->GetObject());
    const auto prop_index = *static_cast<int*>(gen->GetAddressOfArg(0));
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

    entity->SetValueAsFloat(prop, *static_cast<float*>(gen->GetAddressOfArg(1)));
#else
    UNUSED_VARIABLE(gen);
#endif
}

static void Property_GetValue(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto* entity = static_cast<Entity*>(gen->GetObject());
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

static void Property_SetValue(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<Entity*>(gen->GetObject());
    const auto* prop = static_cast<const Property*>(gen->GetAuxiliary());
    ENTITY_VERIFY(entity);

    auto& props = entity->GetPropertiesForEdit();
    const auto* data = static_cast<const uchar*>(gen->GetAddressOfArg(0));

    if (prop->IsPlainData()) {
        props.SetRawData(prop, data, prop->GetBaseSize());
    }
    else if (prop->IsString()) {
        const auto& str = *reinterpret_cast<const string*>(data);
        props.SetRawData(prop, reinterpret_cast<const uchar*>(str.data()), static_cast<uint>(str.length()));
    }
    else if (prop->IsArray()) {
        const auto* arr = reinterpret_cast<const CScriptArray*>(data);

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
        const auto* dict = reinterpret_cast<const CScriptDict*>(data);

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

template<class T>
static Entity* EntityDownCast(T* a)
{
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return static_cast<Entity*>(a);
}

template<class T>
static T* EntityUpCast(Entity* a)
{
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return dynamic_cast<T*>(a);
}

static void HashedString_Construct(hstring* mem)
{
    new (mem) hstring();
}

static void HashedString_ConstructFromString(asIScriptGeneric* gen)
{
    new (gen->GetAddressOfReturnLocation()) hstring();
}

static void HashedString_ConstructFromHash(asIScriptGeneric* gen)
{
    new (gen->GetAddressOfReturnLocation()) hstring();
}

static void HashedString_ConstructCopy(const hstring& self, hstring* mem)
{
    new (mem) hstring(self);
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

    WriteLog("{}\n", formatted_message);
}

#if COMPILER_MODE
static void CompileRootModule(asIScriptEngine* engine, string_view script_path);
#else
static void RestoreRootModule(asIScriptEngine* engine, File& script_file);
#endif

void SCRIPTING_CLASS::InitAngelScriptScripting(INIT_ARGS)
{
#if !COMPILER_MODE
    FOEngine* game_engine = _engine;
#else
#if SERVER_SCRIPTING
    FOEngine* game_engine = new AngelScriptServerCompilerData();
#elif CLIENT_SCRIPTING
    FOEngine* game_engine = new AngelScriptClientCompilerData();
#elif MAPPER_SCRIPTING
    FOEngine* game_engine = new AngelScriptMapperCompilerData();
#endif
#endif

#if COMPILER_MODE
    int dummy = 0;
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
    AS_VERIFY(engine->RegisterObjectType("hstring", sizeof(hstring), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hstring>()));
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
#define REGISTER_BASE_ENTITY(class_name) \
    AS_VERIFY(engine->RegisterObjectType(class_name, 0, asOBJ_REF)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_ADDREF, "void f()", SCRIPT_METHOD(Entity, AddRef), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectBehaviour(class_name, asBEHAVE_RELEASE, "void f()", SCRIPT_METHOD(Entity, Release), SCRIPT_METHOD_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroyed() const", SCRIPT_FUNC_THIS(Entity_IsDestroyed), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "bool get_IsDestroying() const", SCRIPT_FUNC_THIS(Entity_IsDestroying), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "string get_Name() const", SCRIPT_FUNC_THIS(Entity_Name), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterFuncdef("bool " class_name "Predicate(" class_name "@+)")); \
    AS_VERIFY(engine->RegisterFuncdef("void " class_name "Callback(" class_name "@+)")); \
    AS_VERIFY(engine->RegisterFuncdef("void " class_name "InitFunc(" class_name "@+, bool)"))

#define REGISTER_ENTITY_CAST(class_name, real_class, base_class_name) \
    AS_VERIFY(engine->RegisterObjectMethod(base_class_name, class_name "@+ opCast()", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(base_class_name, "const " class_name "@+ opCast() const", SCRIPT_FUNC_THIS((EntityUpCast<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, base_class_name "@+ opImplCast()", SCRIPT_FUNC_THIS((EntityDownCast<Entity>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "const " base_class_name "@+ opImplCast() const", SCRIPT_FUNC_THIS((EntityDownCast<Entity>)), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_GETSET_ENTITY(class_name, prop_class_name) \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsInt(" prop_class_name "Property) const", asFUNCTION(Property_GetValueAsInt), asCALL_GENERIC)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsInt(" prop_class_name "Property, int)", asFUNCTION(Property_SetValueAsInt), asCALL_GENERIC)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsFloat(" prop_class_name "Property) const", asFUNCTION(Property_GetValueAsFloat), asCALL_GENERIC)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsFloat(" prop_class_name "Property, float)", asFUNCTION(Property_SetValueAsFloat), asCALL_GENERIC))

#define REGISTER_GLOBAL_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name); \
    REGISTER_GETSET_ENTITY(class_name, class_name); \
    entity_is_global.insert(class_name)

#define REGISTER_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name); \
    REGISTER_BASE_ENTITY("Abstract" class_name); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Entity"); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY(class_name, class_name); \
    REGISTER_GETSET_ENTITY("Abstract" class_name, class_name); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "uint get_Id() const", SCRIPT_FUNC_THIS(Entity_Id), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("Abstract" class_name, "uint get_Id() const", SCRIPT_FUNC_THIS(Entity_Id), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_ENTITY_PROTO(class_name, proto_real_class) \
    REGISTER_BASE_ENTITY(class_name "Proto"); \
    REGISTER_ENTITY_CAST(class_name "Proto", proto_real_class, "Entity"); \
    REGISTER_ENTITY_CAST(class_name "Proto", proto_real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY(class_name "Proto", class_name); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name "Proto", "hstring get_ProtoId() const", SCRIPT_FUNC_THIS(Entity_ProtoId), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS(Entity_ProtoId), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, class_name "Proto@+ get_Proto() const", SCRIPT_FUNC_THIS(Entity_Proto), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("Abstract" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS(Entity_ProtoId), SCRIPT_FUNC_THIS_CONV)); \
    entity_has_protos.insert(class_name)

#define REGISTER_ENTITY_STATICS(class_name, real_class) \
    REGISTER_BASE_ENTITY("Static" class_name); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Entity"); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY("Static" class_name, class_name); \
    if (entity_has_protos.count(class_name)) { \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS(Entity_ProtoId), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, class_name "Proto@+ get_Proto() const", SCRIPT_FUNC_THIS(Entity_Proto), SCRIPT_FUNC_THIS_CONV)); \
    } \
    entity_has_statics.insert(class_name)

    REGISTER_BASE_ENTITY("Entity");

    unordered_set<string> entity_is_global;
    unordered_set<string> entity_has_protos;
    unordered_set<string> entity_has_statics;

    // Events
#define REGISTER_ENTITY_EVENT(entity_name, event_name, as_args_ent, as_args, func_entry) \
    AS_VERIFY(engine->RegisterFuncdef("void " entity_name event_name "EventFunc(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterFuncdef("bool " entity_name event_name "EventFuncBool(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterObjectType(entity_name event_name "Event", 0, asOBJ_REF | asOBJ_NOHANDLE)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void UnsubscribeAll()", SCRIPT_FUNC_THIS(func_entry##_UnsubscribeAll), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectProperty(entity_name, entity_name event_name "Event On" event_name, 0))

#define REGISTER_ENTITY_EXPORTED_EVENT(entity_name, event_name, as_args_ent, as_args, func_entry) REGISTER_ENTITY_EVENT(entity_name, event_name, as_args_ent, as_args, func_entry)

#define REGISTER_ENTITY_SCRIPT_EVENT(entity_name, event_name, as_args_ent, as_args, func_entry) \
    REGISTER_ENTITY_EVENT(entity_name, event_name, as_args_ent, as_args, func_entry); \
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

        for (const auto i : xrange(registrator->GetCount())) {
            const auto* prop = registrator->GetByIndex(i);
            const auto is_handle = (prop->IsArray() || prop->IsDict());

            if (prop->IsReadable()) {
                const auto decl_get = _str("const {}{} get_{}() const", prop->GetFullTypeName(), is_handle ? "@" : "", prop->GetName()).str();
                AS_VERIFY(engine->RegisterObjectMethod(reg_name.c_str(), decl_get.c_str(), asFUNCTION(Property_GetValue), asCALL_GENERIC, (void*)prop));

                if (!is_global) {
                    AS_VERIFY(engine->RegisterObjectMethod(_str("Abstract{}", reg_name).c_str(), decl_get.c_str(), asFUNCTION(Property_GetValue), asCALL_GENERIC, (void*)prop));
                }
                if (is_has_protos) {
                    AS_VERIFY(engine->RegisterObjectMethod(_str("{}Proto", reg_name).c_str(), decl_get.c_str(), asFUNCTION(Property_GetValue), asCALL_GENERIC, (void*)prop));
                }
                if (is_has_statics) {
                    AS_VERIFY(engine->RegisterObjectMethod(_str("Static{}", reg_name).c_str(), decl_get.c_str(), asFUNCTION(Property_GetValue), asCALL_GENERIC, (void*)prop));
                }
            }

            if (prop->IsWritable()) {
                const auto decl_set = _str("void set_{}({}{}{})", prop->GetName(), is_handle ? "const " : "", prop->GetFullTypeName(), is_handle ? "@+" : "").str();
                AS_VERIFY(engine->RegisterObjectMethod(reg_name.c_str(), decl_set.c_str(), asFUNCTION(Property_SetValue), asCALL_GENERIC, (void*)prop));
            }
        }

        for (const auto& [group_name, properties] : registrator->GetPropertyGroups()) {
            vector<ScriptEnum_uint16> prop_enums;
            prop_enums.reserve(properties.size());
            for (const auto* prop : properties) {
                prop_enums.push_back(static_cast<ScriptEnum_uint16>(prop->GetRegIndex()));
            }

#if !COMPILER_MODE
            void* as_prop_enums = MarshalBackScalarArray(engine, _str("{}Property[]", registrator->GetClassName()).c_str(), prop_enums);
#else
            void* as_prop_enums = &dummy;
#endif
            AS_VERIFY(engine->RegisterGlobalProperty(_str("{}Property[] {}Property{}", registrator->GetClassName(), registrator->GetClassName(), group_name).c_str(), as_prop_enums));
        }
    }

#if SERVER_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ClientCall", 0));
#elif CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterGlobalProperty("RemoteCaller ServerCall", game_engine));
#endif

    AS_VERIFY(engine->RegisterGlobalProperty("Game@ GameInstance", game_engine));

#if CLIENT_SCRIPTING
#if !COMPILER_MODE
    AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", game_engine->_curMap));
    AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", game_engine->_curLocation));
#else
    AS_VERIFY(engine->RegisterGlobalProperty("Map@ CurMap", &dummy));
    AS_VERIFY(engine->RegisterGlobalProperty("Location@ CurLocation", &dummy));
#endif
#endif

#if COMPILER_MODE
    CompileRootModule(engine, script_path);
    engine->ShutDownAndRelease();
#else
    File script_file = _engine->FileSys.ReadFile("ServerRootModule.fosb");
    RestoreRootModule(engine, script_file);
#endif

#if COMPILER_MODE
    game_engine->Release();
#endif
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

#if COMPILER_MODE
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
        WriteLog("Preprocessor message: {}.\n", errors.String);
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

    auto file = DiskFileSystem::OpenFile(string(script_path) + "b", true);
    if (!file) {
        throw ScriptCompilerException("Can't write binary to file", _str("{}b", script_path));
    }
    if (!file.Write(data)) {
        throw ScriptCompilerException("Can't write binary to file", _str("{}b", script_path));
    }
}

#else
static void RestoreRootModule(asIScriptEngine* engine, File& script_file)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);
    RUNTIME_ASSERT(script_file);

    auto reader = DataReader({script_file.GetBuf(), script_file.GetSize()});
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

    BinaryStream binary {buf};
    int as_result = mod->LoadByteCode(&binary);
    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }
}
#endif
