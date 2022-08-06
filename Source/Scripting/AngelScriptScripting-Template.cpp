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
#elif CLIENT_SCRIPTING
#define FOEngine FOClient
#define SCRIPTING_CLASS ClientScriptSystem
#elif SINGLE_SCRIPTING
#define FOEngine FOSingle
#define SCRIPTING_CLASS SingleScriptSystem
#elif MAPPER_SCRIPTING
#define FOEngine FOMapper
#define SCRIPTING_CLASS MapperScriptSystem
#endif
#else
#if !COMPILER_VALIDATION_MODE
#if SERVER_SCRIPTING
#define FOEngine AngelScriptServerCompiler
#define SCRIPTING_CLASS ASCompiler_ServerScriptSystem
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompiler
#define SCRIPTING_CLASS ASCompiler_ClientScriptSystem
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompiler
#define SCRIPTING_CLASS ASCompiler_SingleScriptSystem
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompiler
#define SCRIPTING_CLASS ASCompiler_MapperScriptSystem
#endif
#else
#if SERVER_SCRIPTING
#define FOEngine AngelScriptServerCompilerValidation
#define SCRIPTING_CLASS ASCompiler_ServerScriptSystem_Validation
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompilerValidation
#define SCRIPTING_CLASS ASCompiler_ClientScriptSystem_Validation
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompilerValidation
#define SCRIPTING_CLASS ASCompiler_SingleScriptSystem_Validation
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompilerValidation
#define SCRIPTING_CLASS ASCompiler_MapperScriptSystem_Validation
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
#define INIT_ARGS FileSystem &file_sys, FOEngineBase **out_engine
#else
#define INIT_ARGS FileSystem& file_sys
#endif

struct SCRIPTING_CLASS : public ScriptSystem
{
    void InitAngelScriptScripting(INIT_ARGS);
};

#define ENTITY_VERIFY_NULL(e)
#define ENTITY_VERIFY(e)

static GlobalSettings DummySettings {};

class FOEngine : public FOEngineBase
{
public:
#if SERVER_SCRIPTING
    FOEngine() : FOEngineBase(DummySettings, PropertiesRelationType::ServerRelative)
    {
        extern void AngelScript_ServerCompiler_RegisterData(FOEngineBase*);
        AngelScript_ServerCompiler_RegisterData(this);
    }
#elif CLIENT_SCRIPTING
    FOEngine() : FOEngineBase(DummySettings, PropertiesRelationType::ClientRelative)
    {
        extern void AngelScript_ClientCompiler_RegisterData(FOEngineBase*);
        AngelScript_ClientCompiler_RegisterData(this);
    }
#elif SINGLE_SCRIPTING
    FOEngine() : FOEngineBase(DummySettings, PropertiesRelationType::BothRelative)
    {
        extern void AngelScript_SingleCompiler_RegisterData(FOEngineBase*);
        AngelScript_SingleCompiler_RegisterData(this);
    }
#elif MAPPER_SCRIPTING
    FOEngine() : FOEngineBase(DummySettings, PropertiesRelationType::BothRelative)
    {
        extern void AngelScript_MapperCompiler_RegisterData(FOEngineBase*);
        AngelScript_MapperCompiler_RegisterData(this);
    }
#endif
};
#endif

#if !COMPILER_MODE
#define INIT_ARGS

#define GET_AS_ENGINE_FROM_SELF() static_cast<SCRIPTING_CLASS*>(self->GetEngine()->ScriptSys)->AngelScriptData->Engine
#define GET_SCRIPT_SYS_FROM_SELF() static_cast<SCRIPTING_CLASS*>(self->GetEngine()->ScriptSys)->AngelScriptData.get()

#define ENTITY_VERIFY_NULL(e) \
    if ((e) == nullptr) { \
        throw ScriptException("Access to null entity"); \
    }
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
#define SCRIPT_GENERIC(name) asFUNCTION(name)
#define SCRIPT_GENERIC_CONV asCALL_GENERIC
#define SCRIPT_FUNC(name) WRAP_FN(name)
#define SCRIPT_FUNC_CONV asCALL_GENERIC
#define SCRIPT_FUNC_THIS(name) WRAP_OBJ_FIRST(name)
#define SCRIPT_FUNC_THIS_CONV asCALL_GENERIC
#define SCRIPT_METHOD(type, name) WRAP_MFN(type, name)
#define SCRIPT_METHOD_CONV asCALL_GENERIC
#else
#define SCRIPT_GENERIC(name) asFUNCTION(name)
#define SCRIPT_GENERIC_CONV asCALL_GENERIC
#define SCRIPT_FUNC(name) asFUNCTION(name)
#define SCRIPT_FUNC_CONV asCALL_CDECL
#define SCRIPT_FUNC_THIS(name) asFUNCTION(name)
#define SCRIPT_FUNC_THIS_CONV asCALL_CDECL_OBJFIRST
#define SCRIPT_METHOD(type, name) asMETHOD(type, name)
#define SCRIPT_METHOD_CONV asCALL_THISCALL
#endif

#if !COMPILER_MODE
#define PTR_OR_DUMMY(ptr) (void*)&(ptr)
#else
#define PTR_OR_DUMMY(ptr) (void*)&dummy
#endif

template<typename T>
constexpr bool is_script_enum_v = std::is_same_v<T, ScriptEnum_uint8> || std::is_same_v<T, ScriptEnum_uint16> || std::is_same_v<T, ScriptEnum_int> || std::is_same_v<T, ScriptEnum_uint>;

#if !COMPILER_MODE
static void CallbackException(asIScriptContext* ctx, void* param)
{
    // string str = ctx->GetExceptionString();
    // if (str != "Pass")
    //    HandleException(ctx, _str("Script exception: {}{}", str, !_str(str).endsWith('.') ? "." : ""));
}

struct SCRIPTING_CLASS::AngelScriptImpl
{
    struct ContextData
    {
        string Info {};
        asIScriptContext* Parent {};
        uint SuspendEndTick {};
    };

    struct EnumInfo
    {
        string EnumName {};
        FOEngine* GameEngine {};
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
        ctx_data->SuspendEndTick = 0u;
    }

    auto PrepareContext(asIScriptFunction* func) -> asIScriptContext*
    {
        RUNTIME_ASSERT(func);

        auto* ctx = RequestContext();

        const auto as_result = ctx->Prepare(func);
        if (as_result < 0) {
            ReturnContext(ctx);
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
                const string ex_func_name = ex_func->GetName();
                const auto ex_line = ctx->GetExceptionLineNumber();

                auto* lnt = static_cast<Preprocessor::LineNumberTranslator*>(ex_func->GetModule()->GetUserData());
                const auto ex_orig_file = Preprocessor::ResolveOriginalFile(ex_line, lnt);
                const auto ex_orig_line = Preprocessor::ResolveOriginalLine(ex_line, lnt);

                auto stack_trace = GetContextTraceback(ctx);

                ctx->Abort();
                ReturnContext(ctx);

                throw ScriptException("Script execution exception", ex_string, ex_orig_file, ex_func_name, ex_orig_line, std::move(stack_trace));
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

    void ResumeSuspendedContexts()
    {
        if (BusyContexts.empty()) {
            return;
        }

        vector<asIScriptContext*> resume_contexts;
        const auto tick = GameEngine->GameTime.FrameTick();

        for (auto* ctx : BusyContexts) {
            auto* ctx_data = static_cast<ContextData*>(ctx->GetUserData());
            if ((ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED) && ctx_data->SuspendEndTick != static_cast<uint>(-1) && tick >= ctx_data->SuspendEndTick) {
                resume_contexts.push_back(ctx);
            }
        }

        for (auto* ctx : resume_contexts) {
            try {
                if (RunContext(ctx, true)) {
                    ReturnContext(ctx);
                }
            }
            catch (const std::exception& ex) {
                ReportExceptionAndContinue(ex);
            }
        }
    }

    auto GetContextTraceback(asIScriptContext* top_ctx) -> string
    {
        string result;
        result.reserve(2048);

        auto* ctx = top_ctx;
        while (ctx != nullptr) {
            auto* ctx_data = static_cast<ContextData*>(ctx->GetUserData());

            result += _str("AngelScript stack trace (most recent call first):\n");

            asIScriptFunction* sys_func = ctx->GetSystemFunction();
            if (sys_func != nullptr) {
                result += _str("  {}\n", sys_func->GetDeclaration(true, true, true));
            }

            int line;
            asIScriptFunction* func;

            if (ctx->GetState() == asEXECUTION_EXCEPTION) {
                line = ctx->GetExceptionLineNumber();
                func = ctx->GetExceptionFunction();
            }
            else {
                line = ctx->GetLineNumber(0);
                func = ctx->GetFunction(0);
            }

            if (func != nullptr) {
                auto* lnt = static_cast<Preprocessor::LineNumberTranslator*>(func->GetModule()->GetUserData());
                result += _str("  {} : Line {}\n", func->GetDeclaration(true, true), Preprocessor::ResolveOriginalLine(line, lnt));
            }
            else {
                result += _str("  ??? : Line ???\n");
            }

            const asUINT stack_size = ctx->GetCallstackSize();

            for (asUINT i = 1; i < stack_size; i++) {
                func = ctx->GetFunction(i);
                line = ctx->GetLineNumber(i);
                if (func != nullptr) {
                    auto* lnt = static_cast<Preprocessor::LineNumberTranslator*>(func->GetModule()->GetUserData());
                    result += _str("  {} : Line {}\n", func->GetDeclaration(true, true), Preprocessor::ResolveOriginalLine(line, lnt));
                }
                else {
                    result += _str("  ??? : Line ???\n");
                }
            }

            ctx = ctx_data->Parent;
        }

        return result;
    }

    FOEngine* GameEngine {};
    asIScriptEngine* Engine {};
    set<CScriptArray*> EnumArrays {};
    map<string, EnumInfo> EnumInfos {};
    set<hstring> ContentData {};
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

template<typename T, typename T2 = T>
[[maybe_unused]] static auto MarshalArray(asIScriptEngine* as_engine, CScriptArray* as_array) -> vector<T>
{
    UNUSED_VARIABLE(as_engine);

    if (as_array == nullptr || as_array->GetSize() == 0u) {
        return {};
    }

    vector<T> vec;
    vec.resize(as_array->GetSize());
    for (const auto i : xrange(as_array->GetSize())) {
        vec[i] = static_cast<T>(*static_cast<T2*>(as_array->At(i)));

        if constexpr (std::is_pointer_v<T>) {
            ENTITY_VERIFY(vec[i]);
        }
    }

    return vec;
}

template<typename T, typename T2 = T>
[[maybe_unused]] static void AssignArray(asIScriptEngine* as_engine, const vector<T>& vec, CScriptArray* as_array)
{
    as_array->Resize(0);

    if (!vec.empty()) {
        as_array->Resize(static_cast<asUINT>(vec.size()));

        for (const auto i : xrange(vec)) {
            *static_cast<T2*>(as_array->At(static_cast<asUINT>(i))) = static_cast<T2>(vec[i]);

            if constexpr (std::is_pointer_v<T>) {
                ENTITY_VERIFY(vec[i]);
                if (vec[i] != nullptr) {
                    vec[i]->AddRef();
                }
            }
        }
    }
}

template<typename T, typename T2 = T>
[[maybe_unused]] static auto MarshalBackArray(asIScriptEngine* as_engine, const char* type, const vector<T>& vec) -> CScriptArray*
{
    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        AssignArray<T, T2>(as_engine, vec, as_array);
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

template<typename T, typename U, typename T2 = T, typename U2 = U>
[[maybe_unused]] static auto MarshalDict(asIScriptEngine* as_engine, CScriptDict* as_dict) -> map<T, U>
{
    static_assert(is_script_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring>);
    static_assert(is_script_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring>);

    if (as_dict == nullptr || as_dict->GetSize() == 0u) {
        return {};
    }

    std::vector<std::pair<void*, void*>> dict_data;
    as_dict->GetMap(dict_data);

    map<T, U> map;

    for (auto&& [pkey, pvalue] : dict_data) {
        const auto& key = *static_cast<T2*>(pkey);
        const auto& value = *static_cast<U2*>(pvalue);
        map.emplace(static_cast<T>(key), static_cast<U>(value));
    }

    return map;
}

template<typename T, typename U, typename T2 = T, typename U2 = U>
[[maybe_unused]] static void AssignDict(asIScriptEngine* as_engine, const map<T, U>& map, CScriptDict* as_dict)
{
    static_assert(is_script_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring>);
    static_assert(is_script_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring>);

    as_dict->Clear();

    if (!map.empty()) {
        for (auto&& [key, value] : map) {
            auto k = static_cast<T2>(key);
            auto v = static_cast<U2>(value);
            as_dict->Set(&k, &v);
        }
    }
}

template<typename T, typename U, typename T2 = T, typename U2 = U>
[[maybe_unused]] static auto MarshalBackDict(asIScriptEngine* as_engine, const char* type, const map<T, U>& map) -> CScriptDict*
{
    static_assert(is_script_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring>);
    static_assert(is_script_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring>);

    auto* as_dict = CreateASDict(as_engine, type);

    if (!map.empty()) {
        AssignDict(as_engine, map, as_dict);
    }

    return as_dict;
}

[[maybe_unused]] static auto GetASObjectInfo(void* ptr, int type_id) -> string
{
    switch (type_id) {
    case asTYPEID_VOID:
        return "void";
    case asTYPEID_BOOL:
        return _str("bool: {}", *static_cast<bool*>(ptr));
    case asTYPEID_INT8:
        return _str("int8: {}", *static_cast<char*>(ptr));
    case asTYPEID_INT16:
        return _str("int16: {}", *static_cast<short*>(ptr));
    case asTYPEID_INT32:
        return _str("int: {}", *static_cast<int*>(ptr));
    case asTYPEID_INT64:
        return _str("int64: {}", *static_cast<int64*>(ptr));
    case asTYPEID_UINT8:
        return _str("uint8: {}", *static_cast<uchar*>(ptr));
    case asTYPEID_UINT16:
        return _str("uint16: {}", *static_cast<ushort*>(ptr));
    case asTYPEID_UINT32:
        return _str("uint: {}", *static_cast<uint*>(ptr));
    case asTYPEID_UINT64:
        return _str("uint64: {}", *static_cast<uint64*>(ptr));
    case asTYPEID_FLOAT:
        return _str("float: {}", *static_cast<float*>(ptr));
    case asTYPEID_DOUBLE:
        return _str("double: {}", *static_cast<double*>(ptr));
    default:
        break;
    }

    // Todo: GetASObjectInfo add detailed info about object
    auto* ctx = asGetActiveContext();
    RUNTIME_ASSERT(ctx);
    auto* engine = ctx->GetEngine();
    auto* type_info = engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(type_info);
    const string type_name = type_info->GetName();

    if (type_name == "string") {
        return _str("string: {}", *static_cast<string*>(ptr));
    }
    if (type_name == "hstring") {
        return _str("hstring: {}", *static_cast<hstring*>(ptr));
    }
    return _str("{}", type_name);
}
#endif

[[maybe_unused]] static auto GetASFuncName(const asIScriptFunction* func, NameResolver& name_resolver) -> hstring
{
    if (func == nullptr) {
        return hstring();
    }

    string func_name;

    if (func->GetNamespace() == nullptr) {
        func_name = _str("{}", func->GetName()).str();
    }
    else {
        func_name = _str("{}::{}", func->GetNamespace(), func->GetName()).str();
    }

    return name_resolver.ToHashedString(func_name);
}

#if !COMPILER_MODE
static auto CalcConstructAddrSpace(const Property* prop) -> size_t
{
    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            return sizeof(hstring);
        }
        else if (prop->IsBaseTypeEnum()) {
            return sizeof(int);
        }
        else {
            return prop->GetBaseSize();
        }
    }
    else if (prop->IsString()) {
        return sizeof(string);
    }
    else if (prop->IsArray()) {
        return sizeof(void*);
    }
    else if (prop->IsDict()) {
        return sizeof(void*);
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
}

static void FreeConstructAddrSpace(const Property* prop, void* construct_addr)
{
    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            (*static_cast<hstring*>(construct_addr)).~hstring();
        }
    }
    else if (prop->IsString()) {
        (*static_cast<string*>(construct_addr)).~string();
    }
    else if (prop->IsArray()) {
        (*static_cast<CScriptArray**>(construct_addr))->Release();
    }
    else if (prop->IsDict()) {
        (*static_cast<CScriptDict**>(construct_addr))->Release();
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
}

static void PropsToAS(const Property* prop, PropertyRawData& prop_data, void* construct_addr, asIScriptEngine* as_engine)
{
    const auto resolve_hash = [prop](const void* hptr) -> hstring {
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(hptr);
        const auto& name_resolver = prop->GetRegistrator()->GetNameResolver();
        return name_resolver.ResolveHash(hash);
    };

    const auto* data = prop_data.GetPtrAs<uchar>();
    const auto data_size = prop_data.GetSize();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            RUNTIME_ASSERT(data_size == sizeof(hstring::hash_t));
            new (construct_addr) hstring(resolve_hash(data));
        }
        else {
            RUNTIME_ASSERT(data_size != 0);
            std::memcpy(construct_addr, data, data_size);
        }
    }
    else if (prop->IsString()) {
        new (construct_addr) string(reinterpret_cast<const char*>(data), data_size);
    }
    else if (prop->IsArray()) {
        auto* arr = CreateASArray(as_engine, prop->GetFullTypeName().c_str());

        if (prop->IsArrayOfString()) {
            if (data_size != 0u) {
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
        else if (prop->IsBaseTypeHash()) {
            if (data_size != 0u) {
                RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

                const auto count = data_size / prop->GetBaseSize();
                arr->Resize(count);

                for (const auto i : xrange(count)) {
                    const auto hvalue = resolve_hash(data);
                    arr->SetValue(i, (void*)&hvalue);

                    data += sizeof(hstring::hash_t);
                }
            }
        }
        else {
            if (data_size != 0u) {
                arr->Resize(data_size / prop->GetBaseSize());

                std::memcpy(arr->At(0), data, data_size);
            }
        }

        *static_cast<CScriptArray**>(construct_addr) = arr;
    }
    else if (prop->IsDict()) {
        CScriptDict* dict = CreateASDict(as_engine, prop->GetFullTypeName().c_str());

        if (data_size != 0u) {
            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;
                    data += prop->GetDictKeySize();

                    uint arr_size;
                    std::memcpy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(arr_size);

                    auto* arr = CreateASArray(as_engine, _str("{}[]", prop->GetBaseTypeName()).c_str());

                    if (arr_size != 0u) {
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
                        else if (prop->IsBaseTypeHash()) {
                            RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hstring::hash_t));

                            arr->Resize(arr_size);

                            for (const auto i : xrange(arr_size)) {
                                const auto hvalue = resolve_hash(data);
                                arr->SetValue(i, (void*)&hvalue);

                                data += sizeof(hstring::hash_t);
                            }
                        }
                        else {
                            arr->Resize(arr_size);

                            std::memcpy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            data += arr_size * prop->GetBaseSize();
                        }
                    }

                    if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set((void*)&hkey, &arr);
                    }
                    else {
                        dict->Set((void*)key, &arr);
                    }

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

                    if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set((void*)&hkey, &str);
                    }
                    else {
                        dict->Set((void*)key, &str);
                    }

                    data += str_size;
                }
            }
            else {
                const auto whole_element_size = prop->GetDictKeySize() + prop->GetBaseSize();
                const auto dict_size = data_size / whole_element_size;

                for (uint i = 0; i < dict_size; i++) {
                    const auto* key = data + i * whole_element_size;
                    const auto* value = data + i * whole_element_size + prop->GetDictKeySize();

                    if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(value);
                            dict->Set((void*)&hkey, (void*)&hvalue);
                        }
                        else {
                            dict->Set((void*)&hkey, (void*)&value);
                        }
                    }
                    else {
                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(value);
                            dict->Set((void*)key, (void*)&hvalue);
                        }
                        else {
                            dict->Set((void*)key, (void*)&value);
                        }
                    }
                }
            }
        }

        *static_cast<CScriptDict**>(construct_addr) = dict;
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
}

static auto ASToProps(const Property* prop, void* as_obj) -> PropertyRawData
{
    PropertyRawData prop_data;

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            const auto hash = static_cast<const hstring*>(as_obj)->as_hash();
            RUNTIME_ASSERT(prop->GetBaseSize() == sizeof(hash));
            prop_data.SetAs<hstring::hash_t>(hash);
        }
        else {
            prop_data.Set(as_obj, prop->GetBaseSize());
        }
    }
    else if (prop->IsString()) {
        const auto& str = *static_cast<const string*>(as_obj);
        prop_data.Pass(str.data(), str.length());
    }
    else if (prop->IsArray()) {
        const auto* arr = *static_cast<CScriptArray**>(as_obj);

        if (prop->IsArrayOfString()) {
            const auto arr_size = (arr != nullptr ? arr->GetSize() : 0u);

            if (arr_size != 0u) {
                // Calculate size
                size_t data_size = sizeof(uint);

                for (uint i = 0; i < arr_size; i++) {
                    const auto& str = *static_cast<const string*>(arr->At(i));
                    data_size += sizeof(uint) + static_cast<uint>(str.length());
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                std::memcpy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint);

                for (const auto i : xrange(arr_size)) {
                    const auto& str = *static_cast<const string*>(arr->At(i));

                    uint str_size = static_cast<uint>(str.length());
                    std::memcpy(buf, &str_size, sizeof(str_size));
                    buf += sizeof(str_size);

                    if (str_size != 0u) {
                        std::memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else {
            size_t data_size = (arr != nullptr ? arr->GetSize() * prop->GetBaseSize() : 0u);

            if (data_size != 0u) {
                if (prop->IsBaseTypeHash()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (const auto i : xrange(arr->GetSize())) {
                        const auto hash = static_cast<const hstring*>(arr->At(i))->as_hash();
                        std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), sizeof(hstring::hash_t));
                        buf += prop->GetBaseSize();
                    }
                }
                else {
                    prop_data.Pass(arr->At(0), data_size);
                }
            }
        }
    }
    else if (prop->IsDict()) {
        const auto* dict = *static_cast<CScriptDict**>(as_obj);
        if (prop->IsDictOfArray()) {
            if (dict != nullptr && dict->GetSize() != 0u) {
                // Calculate size
                size_t data_size = 0u;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
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
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict_map) {
                    const auto* arr = *static_cast<const CScriptArray**>(value);
                    if (prop->IsDictKeyHash()) {
                        const auto hash = static_cast<const hstring*>(key)->as_hash();
                        std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), prop->GetDictKeySize());
                    }
                    else {
                        std::memcpy(buf, key, prop->GetDictKeySize());
                    }
                    buf += prop->GetDictKeySize();

                    const uint arr_size = (arr != nullptr ? arr->GetSize() : 0u);
                    std::memcpy(buf, &arr_size, sizeof(uint));
                    buf += sizeof(arr_size);

                    if (arr_size != 0u) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint i = 0; i < arr_size; i++) {
                                const auto& str = *static_cast<const string*>(arr->At(i));
                                const auto str_size = static_cast<uint>(str.length());

                                std::memcpy(buf, &str_size, sizeof(uint));
                                buf += sizeof(str_size);

                                if (str_size != 0u) {
                                    std::memcpy(buf, str.c_str(), str_size);
                                    buf += arr_size;
                                }
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            for (uint i = 0; i < arr_size; i++) {
                                const auto hash = static_cast<const hstring*>(arr->At(i))->as_hash();
                                std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), sizeof(hstring::hash_t));
                                buf += sizeof(hstring::hash_t);
                            }
                        }
                        else {
                            std::memcpy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                            buf += arr_size * prop->GetBaseSize();
                        }
                    }
                }
            }
        }
        else if (prop->IsDictOfString()) {
            if (dict != nullptr && dict->GetSize() != 0u) {
                // Calculate size
                size_t data_size = 0u;
                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
                    const auto& str = *static_cast<const string*>(value);
                    const auto str_size = static_cast<uint>(str.length());

                    data_size += prop->GetDictKeySize() + sizeof(str_size) + str_size;
                }

                // Make buffer
                uchar* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict_map) {
                    const auto& str = *static_cast<const string*>(value);
                    if (prop->IsDictKeyHash()) {
                        const auto hash = static_cast<const hstring*>(key)->as_hash();
                        std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), prop->GetDictKeySize());
                    }
                    else {
                        std::memcpy(buf, key, prop->GetDictKeySize());
                    }
                    buf += prop->GetDictKeySize();

                    const auto str_size = static_cast<uint>(str.length());
                    std::memcpy(buf, &str_size, sizeof(uint));
                    buf += sizeof(str_size);

                    if (str_size != 0u) {
                        std::memcpy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeySize();
            const auto value_element_size = prop->GetBaseSize();
            const auto whole_element_size = key_element_size + value_element_size;
            const auto data_size = (dict != nullptr ? dict->GetSize() * whole_element_size : 0u);

            if (data_size != 0u) {
                auto* buf = prop_data.Alloc(data_size);

                vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
                    if (prop->IsDictKeyHash()) {
                        const auto hash = static_cast<const hstring*>(key)->as_hash();
                        std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), key_element_size);
                    }
                    else {
                        std::memcpy(buf, key, key_element_size);
                    }
                    buf += key_element_size;

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = static_cast<const hstring*>(value)->as_hash();
                        std::memcpy(buf, reinterpret_cast<const uchar*>(&hash), value_element_size);
                    }
                    else {
                        std::memcpy(buf, value, value_element_size);
                    }
                    buf += value_element_size;
                }
            }
        }
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }

    return prop_data;
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static auto CalcNetBufParamLen(const T& value) -> uint
{
    if constexpr (std::is_same_v<T, string>) {
        if (value.size() > 0xFFFF) {
            throw ScriptException("Too big string to send", value.length());
        }

        return NetBuffer::STRING_LEN_SIZE + static_cast<uint>(value.length());
    }
    else if constexpr (std::is_same_v<T, hstring>) {
        return sizeof(hstring::hash_t);
    }
    else if constexpr (std::is_arithmetic_v<T> || is_script_enum_v<T>) {
        return sizeof(value);
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static auto CalcNetBufParamLen(const vector<T>& value) -> uint
{
    if (value.size() > 0xFFFF) {
        throw ScriptException("Too big array to send", value.size());
    }

    if constexpr (std::is_same_v<T, string>) {
        uint result = 0u;
        for (const auto& inner_value : value) {
            result += CalcNetBufParamLen(inner_value);
        }
        return NetBuffer::ARRAY_LEN_SIZE + result;
    }
    else if constexpr (std::is_same_v<T, hstring>) {
        return NetBuffer::ARRAY_LEN_SIZE + static_cast<uint>(value.size() * sizeof(hstring::hash_t));
    }
    else if constexpr (std::is_arithmetic_v<T> || is_script_enum_v<T>) {
        return NetBuffer::ARRAY_LEN_SIZE + static_cast<uint>(value.size() * sizeof(T));
    }
}

template<typename T, typename U,
    std::enable_if_t<(std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>)&& //
        (std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_arithmetic_v<U> || is_script_enum_v<U>),
        int> = 0>
static auto CalcNetBufParamLen(const map<T, U>& value) -> uint
{
    if (value.size() > 0xFFFF) {
        throw ScriptException("Too big dict to send", value.size());
    }

    uint result = NetBuffer::ARRAY_LEN_SIZE;

    if constexpr (std::is_same_v<T, hstring>) {
        result += static_cast<uint>(value.size() * sizeof(hstring::hash_t));
    }
    else if constexpr (std::is_arithmetic_v<T> || is_script_enum_v<T>) {
        result += static_cast<uint>(value.size() * sizeof(T));
    }

    if constexpr (std::is_same_v<U, string>) {
        for (const auto& inner_value : value) {
            result += CalcNetBufParamLen(inner_value.second);
        }
    }
    else if constexpr (std::is_same_v<U, hstring>) {
        result += static_cast<uint>(value.size() * sizeof(hstring::hash_t));
    }
    else if constexpr (std::is_arithmetic_v<U> || is_script_enum_v<U>) {
        result += static_cast<uint>(value.size() * sizeof(T));
    }

    return result;
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const T& value)
{
    if constexpr (std::is_same_v<T, string>) {
        out_buf << static_cast<ushort>(value.length());
        out_buf.Push(value.data(), static_cast<uint>(value.length()));
    }
    else if constexpr (std::is_same_v<T, hstring>) {
        out_buf << value;
    }
    else if constexpr (std::is_arithmetic_v<T> || is_script_enum_v<T>) {
        out_buf << value;
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const vector<T>& value)
{
    out_buf << static_cast<ushort>(value.size());

    for (const auto& inner_value : value) {
        WriteNetBuf(out_buf, inner_value);
    }
}

template<typename T, typename U,
    std::enable_if_t<(std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>)&& //
        (std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_arithmetic_v<U> || is_script_enum_v<U>),
        int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const map<T, U>& value)
{
    out_buf << static_cast<ushort>(value.size());

    for (const auto& inner_value : value) {
        WriteNetBuf(out_buf, inner_value.first);
        WriteNetBuf(out_buf, inner_value.second);
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, T& value, NameResolver& name_resolver)
{
    if constexpr (std::is_same_v<T, string>) {
        ushort len = 0;
        in_buf >> len;
        value.resize(len);
        in_buf.Pop(value.data(), len);
    }
    else if constexpr (std::is_same_v<T, hstring>) {
        value = in_buf.ReadHashedString(name_resolver);
    }
    else if constexpr (std::is_arithmetic_v<T> || is_script_enum_v<T>) {
        in_buf >> value;
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>, int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, vector<T>& value, NameResolver& name_resolver)
{
    ushort inner_values_count = 0;
    in_buf >> inner_values_count;
    value.reserve(inner_values_count);

    for (ushort i = 0; i < inner_values_count; i++) {
        T inner_value;
        ReadNetBuf(in_buf, inner_value, name_resolver);
        value.emplace_back(inner_value);
    }
}

template<typename T, typename U,
    std::enable_if_t<(std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || is_script_enum_v<T>)&& //
        (std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_arithmetic_v<U> || is_script_enum_v<U>),
        int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, map<T, U>& value, NameResolver& name_resolver)
{
    ushort inner_values_count = 0;
    in_buf >> inner_values_count;

    for (ushort i = 0; i < inner_values_count; i++) {
        T inner_value_first;
        ReadNetBuf(in_buf, inner_value_first, name_resolver);
        U inner_value_second;
        ReadNetBuf(in_buf, inner_value_second, name_resolver);
        value.emplace(inner_value_first, inner_value_second);
    }
}
#endif

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
    ENTITY_VERIFY_NULL(self);
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
    ENTITY_VERIFY_NULL(self);
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
    ENTITY_VERIFY_NULL(self);
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
    ENTITY_VERIFY_NULL(self);
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
    ENTITY_VERIFY_NULL(self);
    ENTITY_VERIFY(self);
    return self->GetProto();
#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

///@ CodeGen Global

static void Global_Assert_0(bool condition)
{
#if !COMPILER_MODE
    if (!condition) {
        throw ScriptException("Assertion failed");
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_1(bool condition, void* obj1Ptr, int obj1)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        throw ScriptException("Assertion failed", obj_info1);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_2(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        throw ScriptException("Assertion failed", obj_info1, obj_info2);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_3(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_4(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_5(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_6(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_7(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
        auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_8(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
        auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
        auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_9(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8, void* obj9Ptr, int obj9)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
        auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
        auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
        auto&& obj_info9 = GetASObjectInfo(obj9Ptr, obj9);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8, obj_info9);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Assert_10(bool condition, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8, void* obj9Ptr, int obj9, void* obj10Ptr, int obj10)
{
#if !COMPILER_MODE
    if (!condition) {
        auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
        auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
        auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
        auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
        auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
        auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
        auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
        auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
        auto&& obj_info9 = GetASObjectInfo(obj9Ptr, obj9);
        auto&& obj_info10 = GetASObjectInfo(obj10Ptr, obj10);
        throw ScriptException("Assertion failed", obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8, obj_info9, obj_info10);
    }
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_0(string message)
{
#if !COMPILER_MODE
    throw ScriptException(message);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_1(string message, void* obj1Ptr, int obj1)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    throw ScriptException(message, obj_info1);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_2(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    throw ScriptException(message, obj_info1, obj_info2);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_3(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_4(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_5(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_6(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_7(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
    auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_8(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
    auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
    auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_9(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8, void* obj9Ptr, int obj9)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
    auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
    auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
    auto&& obj_info9 = GetASObjectInfo(obj9Ptr, obj9);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8, obj_info9);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_ThrowException_10(string message, void* obj1Ptr, int obj1, void* obj2Ptr, int obj2, void* obj3Ptr, int obj3, void* obj4Ptr, int obj4, void* obj5Ptr, int obj5, void* obj6Ptr, int obj6, void* obj7Ptr, int obj7, void* obj8Ptr, int obj8, void* obj9Ptr, int obj9, void* obj10Ptr, int obj10)
{
#if !COMPILER_MODE
    auto&& obj_info1 = GetASObjectInfo(obj1Ptr, obj1);
    auto&& obj_info2 = GetASObjectInfo(obj2Ptr, obj2);
    auto&& obj_info3 = GetASObjectInfo(obj3Ptr, obj3);
    auto&& obj_info4 = GetASObjectInfo(obj4Ptr, obj4);
    auto&& obj_info5 = GetASObjectInfo(obj5Ptr, obj5);
    auto&& obj_info6 = GetASObjectInfo(obj6Ptr, obj6);
    auto&& obj_info7 = GetASObjectInfo(obj7Ptr, obj7);
    auto&& obj_info8 = GetASObjectInfo(obj8Ptr, obj8);
    auto&& obj_info9 = GetASObjectInfo(obj9Ptr, obj9);
    auto&& obj_info10 = GetASObjectInfo(obj10Ptr, obj10);
    throw ScriptException(message, obj_info1, obj_info2, obj_info3, obj_info4, obj_info5, obj_info6, obj_info7, obj_info8, obj_info9, obj_info10);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Yield(uint time)
{
#if !COMPILER_MODE
    auto* ctx = asGetActiveContext();
    RUNTIME_ASSERT(ctx);
    auto* ctx_data = static_cast<SCRIPTING_CLASS::AngelScriptImpl::ContextData*>(ctx->GetUserData());
    auto* game_engine = static_cast<FOEngine*>(ctx->GetEngine()->GetUserData());
    ctx_data->SuspendEndTick = time != static_cast<uint>(-1) ? game_engine->GameTime.FrameTick() + time : static_cast<uint>(-1);
    ctx->Suspend();
#else
    throw ScriptCompilerException("Stub");
#endif
}

#if !COMPILER_MODE
static auto ASGenericCall(SCRIPTING_CLASS::AngelScriptImpl* script_sys, GenericScriptFunc* gen_func, asIScriptFunction* func, initializer_list<void*> args, void* ret) -> bool
{
    static unordered_map<const std::type_info*, std::function<void(asIScriptContext*, asUINT, void*)>> CtxSetValueMap = {
        {&typeid(bool), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgByte(index, *static_cast<bool*>(ptr)); }},
        {&typeid(char), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgByte(index, *static_cast<char*>(ptr)); }},
        {&typeid(short), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgWord(index, *static_cast<short*>(ptr)); }},
        {&typeid(int), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgDWord(index, *static_cast<int*>(ptr)); }},
        {&typeid(int64), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgQWord(index, *static_cast<int64*>(ptr)); }},
        {&typeid(uchar), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgByte(index, *static_cast<uchar*>(ptr)); }},
        {&typeid(ushort), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgWord(index, *static_cast<ushort*>(ptr)); }},
        {&typeid(uint), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgDWord(index, *static_cast<uint*>(ptr)); }},
        {&typeid(uint64), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgQWord(index, *static_cast<uint64*>(ptr)); }},
        {&typeid(float), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgFloat(index, *static_cast<float*>(ptr)); }},
        {&typeid(double), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgDouble(index, *static_cast<double*>(ptr)); }},
#if SERVER_SCRIPTING
        {&typeid(Player*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<Player**>(ptr)); }},
        {&typeid(Item*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<Item**>(ptr)); }},
        {&typeid(StaticItem*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<StaticItem**>(ptr)); }},
        {&typeid(Critter*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<Critter**>(ptr)); }},
        {&typeid(Map*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<Map**>(ptr)); }},
        {&typeid(Location*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<Location**>(ptr)); }},
        {&typeid(vector<Player*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Player[]", *static_cast<vector<Player*>*>(ptr))); }},
        {&typeid(vector<Item*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Item[]", *static_cast<vector<Item*>*>(ptr))); }},
        {&typeid(vector<StaticItem*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "StaticItem[]", *static_cast<vector<StaticItem*>*>(ptr))); }},
        {&typeid(vector<Critter*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Critter[]", *static_cast<vector<Critter*>*>(ptr))); }},
        {&typeid(vector<Map*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Map[]", *static_cast<vector<Map*>*>(ptr))); }},
        {&typeid(vector<Location*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Location[]", *static_cast<vector<Location*>*>(ptr))); }},
#else
        {&typeid(PlayerView*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<PlayerView**>(ptr)); }},
        {&typeid(ItemView*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<ItemView**>(ptr)); }},
        {&typeid(CritterView*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<CritterView**>(ptr)); }},
        {&typeid(MapView*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<MapView**>(ptr)); }},
        {&typeid(LocationView*), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, *static_cast<LocationView**>(ptr)); }},
        {&typeid(vector<PlayerView*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Player[]", *static_cast<vector<PlayerView*>*>(ptr))); }},
        {&typeid(vector<ItemView*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Item[]", *static_cast<vector<ItemView*>*>(ptr))); }},
        {&typeid(vector<CritterView*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Critter[]", *static_cast<vector<CritterView*>*>(ptr))); }},
        {&typeid(vector<MapView*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Map[]", *static_cast<vector<MapView*>*>(ptr))); }},
        {&typeid(vector<LocationView*>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "Location[]", *static_cast<vector<LocationView*>*>(ptr))); }},
#endif
        {&typeid(vector<bool>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "bool[]", *static_cast<vector<bool>*>(ptr))); }},
        {&typeid(vector<char>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "int8[]", *static_cast<vector<char>*>(ptr))); }},
        {&typeid(vector<short>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "int16[]", *static_cast<vector<short>*>(ptr))); }},
        {&typeid(vector<int>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "int[]", *static_cast<vector<int>*>(ptr))); }},
        {&typeid(vector<int64>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "int64[]", *static_cast<vector<int64>*>(ptr))); }},
        {&typeid(vector<uchar>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "uint8[]", *static_cast<vector<uchar>*>(ptr))); }},
        {&typeid(vector<ushort>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "uint16[]", *static_cast<vector<ushort>*>(ptr))); }},
        {&typeid(vector<uint>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "uint[]", *static_cast<vector<uint>*>(ptr))); }},
        {&typeid(vector<uint64>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "uint64[]", *static_cast<vector<uint64>*>(ptr))); }},
        {&typeid(vector<float>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "float[]", *static_cast<vector<float>*>(ptr))); }},
        {&typeid(vector<double>), [](asIScriptContext* ctx, asUINT index, void* ptr) { ctx->SetArgObject(index, MarshalBackArray(ctx->GetEngine(), "double[]", *static_cast<vector<double>*>(ptr))); }},
    };

    RUNTIME_ASSERT(gen_func);
    RUNTIME_ASSERT(func);
    RUNTIME_ASSERT(gen_func->ArgsType.size() == args.size());
    RUNTIME_ASSERT(gen_func->ArgsType.size() == func->GetParamCount());

    if (ret != nullptr) {
        RUNTIME_ASSERT(func->GetReturnTypeId() != asTYPEID_VOID);
        RUNTIME_ASSERT(gen_func->RetType != &typeid(void));
    }
    else {
        RUNTIME_ASSERT(func->GetReturnTypeId() == asTYPEID_VOID);
        RUNTIME_ASSERT(gen_func->RetType == &typeid(void));
    }

    auto* ctx = script_sys->PrepareContext(func);

    if (args.size() != 0u) {
        auto it = args.begin();
        for (asUINT i = 0; i < args.size(); i++, it++) {
            CtxSetValueMap[gen_func->ArgsType[i]](ctx, i, *it);
        }
    }

    if (script_sys->RunContext(ctx, ret == nullptr)) {
        if (ret != nullptr) {
            // std::memcpy(ret, func->GetAddressOfReturnLocation(), )
            throw NotImplementedException(LINE_STR);
        }

        script_sys->ReturnContext(ctx);
        return true;
    }

    return false;
}
#endif

template<typename T>
static void ASPropertyGetter(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto* registrator = engine->GetPropertyRegistrator(T::ENTITY_CLASS_NAME);
    const auto prop_index = *static_cast<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));
    const auto* prop = registrator->GetByIndex(static_cast<int>(prop_index));
    auto* as_engine = gen->GetEngine();
    auto* script_sys = static_cast<SCRIPTING_CLASS*>(engine->ScriptSys)->AngelScriptData.get();

    if (auto* type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); type_info == nullptr || type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object");
    }

    auto* func = *reinterpret_cast<asIScriptFunction**>(gen->GetArgAddress(1));

    if (!prop->IsVirtual()) {
        throw ScriptException("Property is not virtual");
    }
    if (func->GetParamCount() != 1 && func->GetParamCount() != 2) {
        throw ScriptException("Invalid getter function");
    }
    if (func->GetReturnTypeId() == asTYPEID_VOID) {
        throw ScriptException("Invalid getter function");
    }
    if (prop->GetFullTypeName() != _str(as_engine->GetTypeDeclaration(func->GetReturnTypeId())).replace("[]@", "[]").str()) {
        throw ScriptException("Invalid getter function");
    }

    int type_id;
    asDWORD flags;
    int as_result;
    AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (auto* type_info = as_engine->GetTypeInfoById(type_id); type_info == nullptr || string_view(type_info->GetName()) != T::ENTITY_CLASS_NAME || flags != 0) {
        throw ScriptException("Invalid getter function");
    }

    if (func->GetParamCount() == 2) {
        AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (type_id != gen->GetArgTypeId(0) || flags != 0) {
            throw ScriptException("Invalid getter function");
        }
    }

    prop->SetGetter([prop, func, script_sys](Entity* entity, const Property*) -> PropertyRawData {
        ENTITY_VERIFY_NULL(entity);
        ENTITY_VERIFY(entity);

        auto* ctx = script_sys->PrepareContext(func);

        if constexpr (std::is_standard_layout_v<T>) {
            auto* actual_entity = dynamic_cast<BaseEntity*>(entity);
            RUNTIME_ASSERT(actual_entity);
            ctx->SetArgObject(0, actual_entity);
        }
        else {
            auto* actual_entity = dynamic_cast<T*>(entity);
            RUNTIME_ASSERT(actual_entity);
            ctx->SetArgObject(0, actual_entity);
        }

        if (func->GetParamCount() == 2) {
            ctx->SetArgDWord(1, prop->GetRegIndex());
        }

        const auto run_ok = script_sys->RunContext(ctx, false);
        RUNTIME_ASSERT(run_ok);

        auto prop_data = ASToProps(prop, ctx->GetAddressOfReturnValue());
        prop_data.StoreIfPassed();
        script_sys->ReturnContext(ctx);
        return prop_data;
    });
#else
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T, typename Deferred>
static void ASPropertySetter(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto* registrator = engine->GetPropertyRegistrator(T::ENTITY_CLASS_NAME);
    const auto prop_index = *static_cast<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));
    const auto* prop = registrator->GetByIndex(static_cast<int>(prop_index));
    auto* as_engine = gen->GetEngine();
    auto* script_sys = static_cast<SCRIPTING_CLASS*>(engine->ScriptSys)->AngelScriptData.get();

    if (auto* type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); type_info == nullptr || type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object");
    }

    auto* func = *reinterpret_cast<asIScriptFunction**>(gen->GetArgAddress(1));

    if (func->GetParamCount() != 1 && func->GetParamCount() != 2 && func->GetParamCount() != 3) {
        throw ScriptException("Invalid setter function");
    }
    if (func->GetReturnTypeId() != asTYPEID_VOID) {
        throw ScriptException("Invalid setter function");
    }

    int type_id;
    asDWORD flags;
    int as_result;
    AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (auto* type_info = as_engine->GetTypeInfoById(type_id); type_info == nullptr || string_view(type_info->GetName()) != T::ENTITY_CLASS_NAME || flags != 0) {
        throw ScriptException("Invalid setter function");
    }

    bool has_proto_enum = false;
    bool has_value_ref = false;

    if (func->GetParamCount() > 1) {
        AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (prop->GetFullTypeName() == _str(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == asTM_INOUTREF) {
            has_value_ref = true;
            if (func->GetParamCount() == 3) {
                throw ScriptException("Invalid setter function");
            }
        }
        else if (type_id == gen->GetArgTypeId(0) && flags == 0) {
            has_proto_enum = true;
        }
        else {
            throw ScriptException("Invalid setter function");
        }

        if (func->GetParamCount() == 3) {
            AS_VERIFY(func->GetParam(2, &type_id, &flags));

            if (prop->GetFullTypeName() == _str(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == asTM_INOUTREF) {
                has_value_ref = true;
            }
            else {
                throw ScriptException("Invalid setter function");
            }
        }
    }

    if constexpr (Deferred::value) {
        if (has_value_ref) {
            throw ScriptException("Invalid setter function");
        }
    }

    prop->AddSetter([prop, func, script_sys, has_proto_enum, has_value_ref](Entity* entity, const Property*, PropertyRawData& prop_data) -> void {
        ENTITY_VERIFY_NULL(entity);
        ENTITY_VERIFY(entity);

        auto* ctx = script_sys->PrepareContext(func);

        if constexpr (std::is_standard_layout_v<T>) {
            auto* actual_entity = dynamic_cast<BaseEntity*>(entity);
            RUNTIME_ASSERT(actual_entity);
            ctx->SetArgObject(0, actual_entity);
        }
        else {
            auto* actual_entity = dynamic_cast<T*>(entity);
            RUNTIME_ASSERT(actual_entity);
            ctx->SetArgObject(0, actual_entity);
        }

        if (has_proto_enum) {
            ctx->SetArgDWord(1, prop->GetRegIndex());
        }

        if constexpr (!Deferred::value) {
            if (has_value_ref) {
                PropertyRawData value_ref_space;
                auto* construct_addr = value_ref_space.Alloc(CalcConstructAddrSpace(prop));
                PropsToAS(prop, prop_data, construct_addr, script_sys->Engine);
                ctx->SetArgAddress(has_proto_enum ? 2 : 1, construct_addr);

                const auto run_ok = script_sys->RunContext(ctx, false);
                RUNTIME_ASSERT(run_ok);

                prop_data = ASToProps(prop, construct_addr);
                prop_data.StoreIfPassed();
                FreeConstructAddrSpace(prop, construct_addr);

                script_sys->ReturnContext(ctx);
            }
            else {
                if (script_sys->RunContext(ctx, true)) {
                    script_sys->ReturnContext(ctx);
                }
            }
        }
        else {
            // Will be run from scheduler
            UNUSED_VARIABLE(has_value_ref);
        }
    });
#else
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Global_Get(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* ptr = *static_cast<T**>(gen->GetAuxiliary());
    *(T**)gen->GetAddressOfReturnLocation() = ptr;
#else
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_GetSelf(T* entity) -> T*
{
#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);
    return entity;
#else
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Property_GetValueAsInt(const T* entity, int prop_index) -> int
{
#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
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
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (prop->IsReadOnly()) {
        throw ScriptException("Property is read only");
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
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
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
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }
    if (prop->IsReadOnly()) {
        throw ScriptException("Property is read only");
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
static void Property_GetComponent(asIScriptGeneric* gen)
{
    auto* entity = static_cast<T*>(gen->GetObject());
    const auto& component = *static_cast<const hstring*>(gen->GetAuxiliary());

    if (auto* entity_with_proto = dynamic_cast<EntityWithProto*>(entity); entity_with_proto != nullptr) {
        if (!entity_with_proto->GetProto()->HasComponent(component)) {
            *(T**)gen->GetAddressOfReturnLocation() = nullptr;
            return;
        }
    }

    *(T**)gen->GetAddressOfReturnLocation() = entity;
}

template<typename T>
static void Property_GetValue(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* entity = static_cast<T*>(gen->GetObject());
    const auto* prop = static_cast<const Property*>(gen->GetAuxiliary());
    auto& getter = prop->GetGetter();
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    PropertyRawData prop_data;

    if (prop->IsVirtual()) {
        if (!getter) {
            throw ScriptException("Getter not set");
        }

        prop_data = getter(entity, prop);
    }
    else {
        uint data_size;
        const auto* data = entity->GetProperties().GetRawData(prop, data_size);
        prop_data.Pass(data, data_size);
    }

    PropsToAS(prop, prop_data, gen->GetAddressOfReturnLocation(), gen->GetEngine());
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
    auto& setters = prop->GetSetters();
    auto& post_setters = prop->GetPostSetters();
    auto& props = entity->GetPropertiesForEdit();
    void* as_obj = gen->GetAddressOfArg(0);
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop->IsVirtual() && setters.empty()) {
        throw ScriptException("Setter not set");
    }

    auto prop_data = ASToProps(prop, as_obj);

    if (!setters.empty()) {
        for (auto& setter : setters) {
            setter(entity, prop, prop_data);
        }
    }

    if (!prop->IsVirtual()) {
        props.SetRawData(prop, prop_data.GetPtrAs<uchar>(), prop_data.GetSize());

        if (!post_setters.empty()) {
            for (auto& setter : post_setters) {
                setter(entity, prop);
            }
        }
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
    const auto* str = *static_cast<const string**>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    auto hstr = engine->ToHashedString(*str);
    new (gen->GetObject()) hstring(hstr);
#endif
}

static void HashedString_CreateFromString(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto* str = *static_cast<const string**>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    auto hstr = engine->ToHashedString(*str);
    new (gen->GetAddressOfReturnLocation()) hstring(hstr);
#endif
}

static void HashedString_CreateFromHash(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    const auto& hash = *static_cast<const uint*>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    auto hstr = engine->ResolveHash(hash);
    new (gen->GetAddressOfReturnLocation()) hstring(hstr);
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

template<typename T, typename U>
static void CustomEntity_Create(asIScriptGeneric* gen)
{
#if !COMPILER_MODE && SERVER_SCRIPTING
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    T* entity = engine->EntityMngr.CreateCustomEntity(U::ENTITY_CLASS_NAME);
    new (gen->GetAddressOfReturnLocation()) T*(entity);
#endif
}

template<typename T, typename U>
static void CustomEntity_Get(asIScriptGeneric* gen)
{
#if !COMPILER_MODE && SERVER_SCRIPTING
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto& entity_id = *static_cast<const uint*>(gen->GetAddressOfArg(0));
    T* entity = engine->EntityMngr.GetCustomEntity(U::ENTITY_CLASS_NAME, entity_id);
    ENTITY_VERIFY(entity);
    new (gen->GetAddressOfReturnLocation()) T*(entity);
#endif
}

template<typename T, typename U>
static void CustomEntity_DeleteById(asIScriptGeneric* gen)
{
#if !COMPILER_MODE && SERVER_SCRIPTING
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto& entity_id = *static_cast<const uint*>(gen->GetAddressOfArg(0));
    engine->EntityMngr.DeleteCustomEntity(U::ENTITY_CLASS_NAME, entity_id);
#endif
}

template<typename T, typename U>
static void CustomEntity_DeleteByRef(asIScriptGeneric* gen)
{
#if !COMPILER_MODE && SERVER_SCRIPTING
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    T* entity = *static_cast<T**>(gen->GetAddressOfArg(0));
    ENTITY_VERIFY(entity);
    if (entity != nullptr) {
        engine->EntityMngr.DeleteCustomEntity(U::ENTITY_CLASS_NAME, entity->GetId());
    }
#endif
}

static void Enum_Parse(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* enum_info = static_cast<SCRIPTING_CLASS::AngelScriptImpl::EnumInfo*>(gen->GetAuxiliary());
    const auto& enum_value_name = *static_cast<string*>(gen->GetAddressOfArg(0));

    bool failed = false;
    int enum_value = enum_info->GameEngine->ResolveEnumValue(enum_info->EnumName, enum_value_name, &failed);

    if (failed) {
        if (gen->GetArgCount() == 2) {
            enum_value = *static_cast<int*>(gen->GetAddressOfArg(1));
        }
        else {
            throw ScriptException("Can't parse enum", enum_info->EnumName, enum_value_name);
        }
    }

    new (gen->GetAddressOfReturnLocation()) int(enum_value);
#else
    throw ScriptCompilerException("Stub");
#endif
}

static void Enum_ToString(asIScriptGeneric* gen)
{
#if !COMPILER_MODE
    auto* enum_info = static_cast<SCRIPTING_CLASS::AngelScriptImpl::EnumInfo*>(gen->GetAuxiliary());
    int enum_index = *static_cast<int*>(gen->GetAddressOfArg(0));
    bool full_spec = *static_cast<bool*>(gen->GetAddressOfArg(1));

    bool failed = false;
    string enum_value_name = enum_info->GameEngine->ResolveEnumValueName(enum_info->EnumName, enum_index, &failed);

    if (failed) {
        throw ScriptException("Invalid enum index", enum_info->EnumName, enum_index);
    }

    if (full_spec) {
        enum_value_name = _str("{}::{}", enum_info->EnumName, enum_value_name);
    }

    new (gen->GetAddressOfReturnLocation()) string(enum_value_name);
#else
    throw ScriptCompilerException("Stub");
#endif
}

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
static void CompileRootModule(asIScriptEngine* engine, FileSystem& file_sys);
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
    AS_VERIFY(engine->SetDefaultNamespace("math"));
    RegisterScriptMath(engine);
    AS_VERIFY(engine->SetDefaultNamespace(""));
    RegisterScriptWeakRef(engine);
    RegisterScriptReflection(engine);

    // Register enums
    for (auto&& [enum_name, enum_pairs] : game_engine->GetAllEnums()) {
        AS_VERIFY(engine->RegisterEnum(enum_name.c_str()));
        for (auto&& [key, value] : enum_pairs) {
            AS_VERIFY(engine->RegisterEnumValue(enum_name.c_str(), key.c_str(), value));
        }

#if !COMPILER_MODE
        auto* enum_info = &AngelScriptData->EnumInfos[enum_name];
        enum_info->EnumName = enum_name;
        enum_info->GameEngine = game_engine;
#else
        void* enum_info = &dummy;
#endif

        AS_VERIFY(engine->SetDefaultNamespace("Enum"));
        AS_VERIFY(engine->RegisterGlobalFunction(_str("{} Parse_{}(string valueName)", enum_name, enum_name).c_str(), SCRIPT_GENERIC(Enum_Parse), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->RegisterGlobalFunction(_str("{} Parse(string valueName, {} defaultValue)", enum_name, enum_name).c_str(), SCRIPT_GENERIC(Enum_Parse), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->RegisterGlobalFunction(_str("string ToString({} value, bool fullSpecification = false)", enum_name).c_str(), SCRIPT_GENERIC(Enum_ToString), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->SetDefaultNamespace(""));
    }

    // Register hstring
    AS_VERIFY(engine->RegisterObjectType("hstring", sizeof(hstring), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<hstring>()));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS(HashedString_Construct), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_ConstructCopy), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const string &in)", SCRIPT_GENERIC(HashedString_ConstructFromString), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromString(const string &in)", SCRIPT_GENERIC(HashedString_CreateFromString), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromHash(int h)", SCRIPT_GENERIC(HashedString_CreateFromHash), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromHash(uint h)", SCRIPT_GENERIC(HashedString_CreateFromHash), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "hstring &opAssign(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_Assign), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const hstring &in) const", SCRIPT_FUNC_THIS(HashedString_Equals), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const string &in) const", SCRIPT_FUNC_THIS(HashedString_EqualsString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string opImplCast() const", SCRIPT_FUNC_THIS(HashedString_StringCast), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string get_str() const", SCRIPT_FUNC_THIS(HashedString_GetString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "int get_hash() const", SCRIPT_FUNC_THIS(HashedString_GetHash), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "uint get_uhash() const", SCRIPT_FUNC_THIS(HashedString_GetUHash), SCRIPT_FUNC_THIS_CONV));

    // Global functions
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition)", SCRIPT_FUNC(Global_Assert_0), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1)", SCRIPT_FUNC(Global_Assert_1), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2)", SCRIPT_FUNC(Global_Assert_2), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3)", SCRIPT_FUNC(Global_Assert_3), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", SCRIPT_FUNC(Global_Assert_4), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", SCRIPT_FUNC(Global_Assert_5), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", SCRIPT_FUNC(Global_Assert_6), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", SCRIPT_FUNC(Global_Assert_7), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", SCRIPT_FUNC(Global_Assert_8), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", SCRIPT_FUNC(Global_Assert_9), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", SCRIPT_FUNC(Global_Assert_10), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message)", SCRIPT_FUNC(Global_ThrowException_0), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1)", SCRIPT_FUNC(Global_ThrowException_1), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2)", SCRIPT_FUNC(Global_ThrowException_2), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3)", SCRIPT_FUNC(Global_ThrowException_3), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", SCRIPT_FUNC(Global_ThrowException_4), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", SCRIPT_FUNC(Global_ThrowException_5), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", SCRIPT_FUNC(Global_ThrowException_6), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", SCRIPT_FUNC(Global_ThrowException_7), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", SCRIPT_FUNC(Global_ThrowException_8), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", SCRIPT_FUNC(Global_ThrowException_9), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", SCRIPT_FUNC(Global_ThrowException_10), SCRIPT_FUNC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Yield(uint duration)", SCRIPT_FUNC(Global_Yield), SCRIPT_FUNC_CONV));

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
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsInt(" prop_class_name "Property prop) const", SCRIPT_FUNC_THIS((Property_GetValueAsInt<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsInt(" prop_class_name "Property prop, int value)", SCRIPT_FUNC_THIS((Property_SetValueAsInt<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "int GetAsFloat(" prop_class_name "Property prop) const", SCRIPT_FUNC_THIS((Property_GetValueAsFloat<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsFloat(" prop_class_name "Property prop, float value)", SCRIPT_FUNC_THIS((Property_SetValueAsFloat<real_class>)), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_ENTITY_PROPS(class_name, real_class) \
    AS_VERIFY(engine->RegisterGlobalFunction("void SetPropertyGetter(" class_name "Property prop, ?&in func)", SCRIPT_GENERIC((ASPropertyGetter<real_class>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterGlobalFunction("void AddPropertySetter(" class_name "Property prop, ?&in func)", SCRIPT_GENERIC((ASPropertySetter<real_class, std::false_type>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterGlobalFunction("void AddPropertyDeferredSetter(" class_name "Property prop, ?&in func)", SCRIPT_GENERIC((ASPropertySetter<real_class, std::true_type>)), SCRIPT_GENERIC_CONV, game_engine))

#define REGISTER_GLOBAL_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name "Singleton", real_class); \
    REGISTER_GETSET_ENTITY(class_name "Singleton", class_name, real_class); \
    REGISTER_ENTITY_PROPS(class_name, real_class); \
    entity_is_global.emplace(class_name); \
    entity_get_component_func_ptr.emplace(class_name "Singleton", SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace(class_name "Singleton", SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name "Singleton", SCRIPT_GENERIC((Property_SetValue<real_class>)))

#define REGISTER_ENTITY(class_name, real_class) \
    REGISTER_BASE_ENTITY(class_name, real_class); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY(class_name, class_name, real_class); \
    REGISTER_ENTITY_PROPS(class_name, real_class); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "uint get_Id() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_SetValue<real_class>)))

#define REGISTER_CUSTOM_ENTITY(class_name, real_class, entity_info) \
    REGISTER_BASE_ENTITY(class_name, real_class); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY(class_name, class_name, real_class); \
    REGISTER_ENTITY_PROPS(class_name, entity_info); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", class_name "@+ Create" class_name "()", SCRIPT_GENERIC((CustomEntity_Create<real_class, entity_info>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", class_name "@+ Get" class_name "(uint id)", SCRIPT_GENERIC((CustomEntity_Get<real_class, entity_info>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "void Delete" class_name "(uint id)", SCRIPT_GENERIC((CustomEntity_DeleteById<real_class, entity_info>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "void Delete" class_name "(" class_name "@+ entity)", SCRIPT_GENERIC((CustomEntity_DeleteByRef<real_class, entity_info>)), SCRIPT_GENERIC_CONV, game_engine)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "uint get_Id() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_SetValue<real_class>)))

#define REGISTER_ENTITY_ABSTRACT(class_name, real_class) \
    REGISTER_BASE_ENTITY("Abstract" class_name, Entity); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY("Abstract" class_name, class_name, Entity); \
    AS_VERIFY(engine->RegisterObjectMethod("Abstract" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<Entity>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace("Abstract" class_name, SCRIPT_GENERIC((Property_GetComponent<Entity>))); \
    entity_get_value_func_ptr.emplace("Abstract" class_name, SCRIPT_GENERIC((Property_GetValue<Entity>))); \
    entity_has_abstract.emplace(class_name)

#define REGISTER_ENTITY_PROTO(class_name, real_class, proto_real_class) \
    REGISTER_BASE_ENTITY("Proto" class_name, proto_real_class); \
    REGISTER_ENTITY_CAST("Proto" class_name, proto_real_class, "Entity"); \
    REGISTER_GETSET_ENTITY("Proto" class_name, class_name, proto_real_class); \
    if (entity_has_abstract.count(class_name) != 0u) { \
        REGISTER_ENTITY_CAST("Proto" class_name, proto_real_class, "Abstract" class_name); \
    } \
    AS_VERIFY(engine->RegisterObjectMethod("Proto" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<proto_real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "Proto" class_name "@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_has_protos.emplace(class_name); \
    entity_get_component_func_ptr.emplace("Proto" class_name, SCRIPT_GENERIC((Property_GetComponent<proto_real_class>))); \
    entity_get_value_func_ptr.emplace("Proto" class_name, SCRIPT_GENERIC((Property_GetValue<proto_real_class>)))

#define REGISTER_ENTITY_STATICS(class_name, real_class) \
    REGISTER_BASE_ENTITY("Static" class_name, real_class); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY("Static" class_name, class_name, real_class); \
    if (entity_has_abstract.count(class_name) != 0u) { \
        REGISTER_ENTITY_CAST("Static" class_name, real_class, "Abstract" class_name); \
    } \
    if (entity_has_protos.count(class_name) != 0u) { \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "Proto" class_name "@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    } \
    entity_has_statics.emplace(class_name); \
    entity_get_component_func_ptr.emplace("Static" class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace("Static" class_name, SCRIPT_GENERIC((Property_GetValue<real_class>)))

#define REGISTER_ENTITY_METHOD(class_name, method_decl, func) AS_VERIFY(engine->RegisterObjectMethod(class_name, method_decl, SCRIPT_FUNC_THIS(func), SCRIPT_FUNC_THIS_CONV))

    REGISTER_BASE_ENTITY("Entity", Entity);

    unordered_set<string> entity_is_global;
    unordered_set<string> entity_has_abstract;
    unordered_set<string> entity_has_protos;
    unordered_set<string> entity_has_statics;
    unordered_map<string, asSFuncPtr> entity_get_component_func_ptr;
    unordered_map<string, asSFuncPtr> entity_get_value_func_ptr;
    unordered_map<string, asSFuncPtr> entity_set_value_func_ptr;

    // Events
#define REGISTER_ENTITY_EVENT(entity_name, class_name, real_class, event_name, as_args_ent, as_args, func_entry) \
    AS_VERIFY(engine->RegisterFuncdef("void " entity_name event_name "EventFunc(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterFuncdef("bool " entity_name event_name "EventFuncBool(" as_args_ent as_args ")")); \
    AS_VERIFY(engine->RegisterObjectType(entity_name event_name "Event", 0, asOBJ_REF | asOBJ_NOCOUNT)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void UnsubscribeAll()", SCRIPT_FUNC_THIS(func_entry##_UnsubscribeAll), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, entity_name event_name "Event@ get_" event_name "()", SCRIPT_FUNC_THIS((Entity_GetSelf<real_class>)), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_ENTITY_EXPORTED_EVENT(entity_name, class_name, real_class, event_name, as_args_ent, as_args, func_entry) REGISTER_ENTITY_EVENT(entity_name, class_name, real_class, event_name, as_args_ent, as_args, func_entry)

#define REGISTER_ENTITY_SCRIPT_EVENT(entity_name, class_name, real_class, event_name, as_args_ent, as_args, func_entry) \
    REGISTER_ENTITY_EVENT(entity_name, class_name, real_class, event_name, as_args_ent, as_args, func_entry); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "bool Fire(" as_args ")", SCRIPT_FUNC_THIS(func_entry##_Fire), SCRIPT_FUNC_THIS_CONV))

    // Settings
    AS_VERIFY(engine->RegisterObjectType("GlobalSettings", 0, asOBJ_REF | asOBJ_NOHANDLE));
    AS_VERIFY(engine->RegisterGlobalProperty("GlobalSettings Settings", game_engine));

#define REGISTER_GET_SETTING(setting_entry, get_decl) AS_VERIFY(engine->RegisterObjectMethod("GlobalSettings", get_decl, SCRIPT_FUNC_THIS(ASSetting_Get_##setting_entry), SCRIPT_FUNC_THIS_CONV))
#define REGISTER_SET_SETTING(setting_entry, set_decl) AS_VERIFY(engine->RegisterObjectMethod("GlobalSettings", set_decl, SCRIPT_FUNC_THIS(ASSetting_Set_##setting_entry), SCRIPT_FUNC_THIS_CONV))

    // Remote calls
#if SERVER_SCRIPTING || CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterObjectType("RemoteCaller", 0, asOBJ_REF | asOBJ_NOHANDLE));

#if SERVER_SCRIPTING
#define PLAYER_ENTITY Player
#else
#define PLAYER_ENTITY PlayerView
#endif

#define BIND_REMOTE_CALL_RECEIVER(name, func_entry, as_func_decl) \
    RUNTIME_ASSERT(_rpcReceivers.count(name##_hash) == 0u); \
    if (auto* func = engine->GetModuleByIndex(0)->GetFunctionByDecl(as_func_decl); func != nullptr) { \
        _rpcReceivers.emplace(name##_hash, [func = RefCountHolder(func)](Entity* entity) { func_entry(static_cast<PLAYER_ENTITY*>(entity), func.get()); }); \
    } \
    else { \
        throw ScriptInitException("Remote call function not found", as_func_decl); \
    }
#endif

    ///@ CodeGen Register

    // Register properties
    for (auto&& [reg_name, registrator] : game_engine->GetAllPropertyRegistrators()) {
        const auto is_global = entity_is_global.count(reg_name) != 0u;
        const auto is_has_abstract = entity_has_abstract.count(reg_name) != 0u;
        const auto is_has_protos = entity_has_protos.count(reg_name) != 0u;
        const auto is_has_statics = entity_has_statics.count(reg_name) != 0u;
        const auto class_name = is_global ? reg_name + "Singleton" : reg_name;
        const auto abstract_class_name = "Abstract" + class_name;
        const auto proto_class_name = "Proto" + class_name;
        const auto static_class_name = "Static" + class_name;
        const auto get_value_func_ptr = entity_get_value_func_ptr.at(class_name);
        const auto get_abstract_value_func_ptr = is_has_abstract ? entity_get_value_func_ptr.at(abstract_class_name) : asSFuncPtr();
        const auto get_proto_value_func_ptr = is_has_protos ? entity_get_value_func_ptr.at(proto_class_name) : asSFuncPtr();
        const auto get_static_value_func_ptr = is_has_statics ? entity_get_value_func_ptr.at(static_class_name) : asSFuncPtr();
        const auto set_value_func_ptr = entity_set_value_func_ptr.at(class_name);
        const auto get_component_func_ptr = entity_get_component_func_ptr.at(class_name);
        const auto get_abstract_component_func_ptr = is_has_abstract ? entity_get_component_func_ptr.at(abstract_class_name) : asSFuncPtr();
        const auto get_proto_component_func_ptr = is_has_protos ? entity_get_component_func_ptr.at(proto_class_name) : asSFuncPtr();
        const auto get_static_component_func_ptr = is_has_statics ? entity_get_component_func_ptr.at(static_class_name) : asSFuncPtr();

        for (const auto& component : registrator->GetComponents()) {
            {
                const auto component_type = _str("{}{}Component", reg_name, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(class_name.c_str(), _str("{}@ get_{}() const", component_type, component).c_str(), get_component_func_ptr, SCRIPT_GENERIC_CONV, (void*)&component));
            }
            if (is_has_abstract) {
                const auto component_type = _str("Abstract{}{}Component", reg_name, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(abstract_class_name.c_str(), _str("{}@ get_{}() const", component_type, component).c_str(), get_abstract_component_func_ptr, SCRIPT_GENERIC_CONV, (void*)&component));
            }
            if (is_has_protos) {
                const auto component_type = _str("Proto{}{}Component", reg_name, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(proto_class_name.c_str(), _str("{}@ get_{}() const", component_type, component).c_str(), get_proto_component_func_ptr, SCRIPT_GENERIC_CONV, (void*)&component));
            }
            if (is_has_statics) {
                const auto component_type = _str("Static{}{}Component", reg_name, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(static_class_name.c_str(), _str("{}@ get_{}() const", component_type, component).c_str(), get_static_component_func_ptr, SCRIPT_GENERIC_CONV, (void*)&component));
            }
        }

        for (const auto i : xrange(registrator->GetCount())) {
            const auto* prop = registrator->GetByIndex(i);
            const auto component = prop->GetComponent();
            const auto is_handle = (prop->IsArray() || prop->IsDict());

            if (!prop->IsDisabled()) {
                const auto decl_get = _str("const {}{} get_{}() const", prop->GetFullTypeName(), is_handle ? "@" : "", prop->GetNameWithoutComponent()).str();
                AS_VERIFY(engine->RegisterObjectMethod(component ? _str("{}{}Component", reg_name, component).c_str() : class_name.c_str(), decl_get.c_str(), get_value_func_ptr, SCRIPT_GENERIC_CONV, (void*)prop));

                if (is_has_abstract) {
                    AS_VERIFY(engine->RegisterObjectMethod(component ? _str("Abstract{}{}Component", reg_name, component).c_str() : abstract_class_name.c_str(), decl_get.c_str(), get_abstract_value_func_ptr, SCRIPT_GENERIC_CONV, (void*)prop));
                }
                if (is_has_protos) {
                    AS_VERIFY(engine->RegisterObjectMethod(component ? _str("Proto{}{}Component", reg_name, component).c_str() : proto_class_name.c_str(), decl_get.c_str(), get_proto_value_func_ptr, SCRIPT_GENERIC_CONV, (void*)prop));
                }
                if (is_has_statics) {
                    AS_VERIFY(engine->RegisterObjectMethod(component ? _str("Static{}{}Component", reg_name, component).c_str() : static_class_name.c_str(), decl_get.c_str(), get_static_value_func_ptr, SCRIPT_GENERIC_CONV, (void*)prop));
                }
            }

            if (!prop->IsDisabled() && !prop->IsReadOnly()) {
                const auto decl_set = _str("void set_{}({}{}{})", prop->GetNameWithoutComponent(), is_handle ? "const " : "", prop->GetFullTypeName(), is_handle ? "@+" : "").str();
                AS_VERIFY(engine->RegisterObjectMethod(component ? _str("{}{}Component", reg_name, component).c_str() : class_name.c_str(), decl_set.c_str(), set_value_func_ptr, SCRIPT_GENERIC_CONV, (void*)prop));
            }
        }

        for (auto&& [group_name, properties] : registrator->GetPropertyGroups()) {
            vector<int> prop_enums;
            prop_enums.reserve(properties.size());
            for (const auto* prop : properties) {
                prop_enums.push_back(prop->GetRegIndex());
            }

            AS_VERIFY(engine->SetDefaultNamespace(_str("{}PropertyGroup", registrator->GetClassName()).c_str()));
#if !COMPILER_MODE
            const auto it_enum = AngelScriptData->EnumArrays.emplace(MarshalBackArray<int>(engine, _str("{}Property[]", registrator->GetClassName()).c_str(), prop_enums));
            RUNTIME_ASSERT(it_enum.second);
#endif
            AS_VERIFY(engine->RegisterGlobalFunction(_str("const {}Property[]@+ get_{}()", registrator->GetClassName(), group_name).c_str(), SCRIPT_GENERIC((Global_Get<CScriptArray>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(*it_enum.first)));
            AS_VERIFY(engine->SetDefaultNamespace(""));
        }
    }

    AS_VERIFY(engine->RegisterGlobalFunction("GameSingleton@+ get_Game()", SCRIPT_GENERIC((Global_Get<FOEngine>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine)));

#if SERVER_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ClientCall", 0));
#elif CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ServerCall", 0));
#endif

#if CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterGlobalFunction("Map@+ get_CurMap()", SCRIPT_GENERIC((Global_Get<MapView>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->CurMap)));
    AS_VERIFY(engine->RegisterGlobalFunction("Location@+ get_CurLocation()", SCRIPT_GENERIC((Global_Get<LocationView>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->_curLocation)));
    AS_VERIFY(engine->RegisterGlobalFunction("Player@+ get_CurPlayer()", SCRIPT_GENERIC((Global_Get<PlayerView>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->_curPlayer)));
#endif

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
    CompileRootModule(engine, file_sys);
    engine->ShutDownAndRelease();
#else
#if COMPILER_VALIDATION_MODE
    game_engine->FileSys.AddDataSource(_str(App->Settings.BakeOutput).combinePath("AngelScript"), DataSourceType::Default);
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

        const auto as_type_to_type_info = [engine](int type_id, asDWORD flags) -> const std::type_info* {
            const auto is_ref = (flags & asTM_INOUTREF) != 0;
            if (is_ref) {
                return nullptr;
            }

            switch (type_id) {
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
            default:
                break;
            }

            if ((type_id & asTYPEID_OBJHANDLE) != 0) {
#define CHECK_CLASS(entity_name, real_class) \
    if (string_view(type_info->GetName()) == entity_name) { \
        return &typeid(real_class*); \
    } \
    if (is_array && type_info->GetSubType() != nullptr && string_view(type_info->GetSubType()->GetName()) == entity_name) { \
        return &typeid(vector<real_class*>); \
    }
#define CHECK_POD_ARRAY(as_type, type) \
    if (is_array && type_info->GetSubTypeId() == as_type) { \
        return &typeid(vector<type>); \
    }
                auto* type_info = engine->GetTypeInfoById(type_id);
                const auto is_array = string_view(type_info->GetName()) == "array";
#if SERVER_SCRIPTING
                CHECK_CLASS("Player", Player);
                CHECK_CLASS("Item", Item);
                CHECK_CLASS("StaticItem", StaticItem);
                CHECK_CLASS("Critter", Critter);
                CHECK_CLASS("Map", Map);
                CHECK_CLASS("Location", Location);
#else
                CHECK_CLASS("Player", PlayerView);
                CHECK_CLASS("Item", ItemView);
                CHECK_CLASS("Critter", CritterView);
                CHECK_CLASS("Map", MapView);
                CHECK_CLASS("Location", LocationView);
#endif
                CHECK_POD_ARRAY(asTYPEID_BOOL, bool);
                CHECK_POD_ARRAY(asTYPEID_INT8, char);
                CHECK_POD_ARRAY(asTYPEID_INT16, short);
                CHECK_POD_ARRAY(asTYPEID_INT32, int);
                CHECK_POD_ARRAY(asTYPEID_INT64, int64);
                CHECK_POD_ARRAY(asTYPEID_UINT8, uchar);
                CHECK_POD_ARRAY(asTYPEID_UINT16, ushort);
                CHECK_POD_ARRAY(asTYPEID_UINT32, uint);
                CHECK_POD_ARRAY(asTYPEID_UINT64, uint64);
                CHECK_POD_ARRAY(asTYPEID_FLOAT, float);
                CHECK_POD_ARRAY(asTYPEID_DOUBLE, double);
#undef CHECK_CLASS
#undef CHECK_POD_ARRAY
            }

            return nullptr;
        };

        for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            auto* func = mod->GetFunctionByIndex(i);

            // Bind
            const auto func_name = GetASFuncName(func, *game_engine);
            const auto it = _funcMap.emplace(func_name, GenericScriptFunc());
            auto& gen_func = it->second;

            gen_func.Name = func_name;
            gen_func.Declaration = func->GetDeclaration(true, true, true);

#if !COMPILER_VALIDATION_MODE
            gen_func.Call = [script_sys = AngelScriptData.get(), gen = &gen_func, func](initializer_list<void*> args, void* ret) { //
                return ASGenericCall(script_sys, gen, func, args, ret);
            };
#endif

            for (asUINT p = 0; p < func->GetParamCount(); p++) {
                int param_type_id;
                asDWORD param_flags;
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

    {
        ///@ CodeGen PostRegister
    }
#endif

#if COMPILER_MODE && COMPILER_VALIDATION_MODE
    RUNTIME_ASSERT(out_engine);
    *out_engine = game_engine;
    game_engine->AddRef();
#endif

#if !COMPILER_MODE
    _loopCallbacks.emplace_back([this] { AngelScriptData->ResumeSuspendedContexts(); });
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
static void CompileRootModule(asIScriptEngine* engine, FileSystem& file_sys)
{
    RUNTIME_ASSERT(engine->GetModuleCount() == 0);

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        ScriptLoader(const string* root, const map<string, string>* files) : _rootScript {root}, _scriptFiles {files} { }

        auto LoadFile(const string& dir, const string& file_name, vector<char>& data, string& file_path) -> bool override
        {
            if (_rootScript != nullptr) {
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
                const auto it = _scriptFiles->find(file_name);
                RUNTIME_ASSERT(it != _scriptFiles->end());

                data.resize(it->second.size());
                std::memcpy(data.data(), it->second.data(), it->second.size());
                return true;
            }

            return Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
        }

        void FileLoaded() override { _includeDeep--; }

    private:
        const string* _rootScript;
        const map<string, string>* _scriptFiles;
        int _includeDeep {};
    };

    auto script_files = file_sys.FilterFiles("fos");

    map<string, string> final_script_files;
    vector<tuple<int, string, string>> final_script_files_order;

    while (script_files.MoveNext()) {
        auto script_file = script_files.GetCurFile();
        auto script_name = script_file.GetName();
        auto script_path = script_file.GetFullPath();
        auto script_content = script_file.GetStr();

        const auto line_sep = script_content.find('\n');
        const auto first_line = script_content.substr(0, line_sep);
        if (first_line.find("// FOS ") == string::npos) {
            throw ScriptCompilerException("No FOS header in script file", script_name);
        }

#if SERVER_SCRIPTING
        if (first_line.find("Common") == string::npos && first_line.find("Server") == string::npos) {
            continue;
        }
#elif CLIENT_SCRIPTING
        if (first_line.find("Common") == string::npos && first_line.find("Client") == string::npos) {
            continue;
        }
#elif SINGLE_SCRIPTING
        if (first_line.find("Common") == string::npos && first_line.find("Single") == string::npos) {
            continue;
        }
#elif MAPPER_SCRIPTING
        if (first_line.find("Common") == string::npos && first_line.find("Mapper") == string::npos) {
            continue;
        }
#endif

        int sort = 0;
        const auto sort_pos = first_line.find("Sort ");
        if (sort_pos != string::npos) {
            sort = _str(first_line.substr(sort_pos + "Sort "_len)).substringUntil(' ').toInt();
        }

        final_script_files_order.push_back(std::make_tuple(sort, script_name, script_path));
        final_script_files.emplace(script_path, std::move(script_content));
    }

    std::sort(final_script_files_order.begin(), final_script_files_order.end(), [](auto& a, auto& b) {
        if (std::get<0>(a) == std::get<0>(b)) {
            return std::get<1>(a) < std::get<1>(b);
        }
        else {
            return std::get<0>(a) < std::get<0>(b);
        }
    });

    string root_script;
    root_script.reserve(final_script_files.size() * 128);

    for (auto&& [script_order, script_name, script_path] : final_script_files_order) {
        root_script.append("namespace ");
        root_script.append(script_name);
        root_script.append(" {\n#include \"");
        root_script.append(script_path);
        root_script.append("\"\n}\n");
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

    auto loader = ScriptLoader(&root_script, &final_script_files);
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

#if SERVER_SCRIPTING
    const string script_out_path = _str(App->Settings.BakeOutput).combinePath("AngelScript/ServerRootModule.fosb");
#elif CLIENT_SCRIPTING
    const string script_out_path = _str(App->Settings.BakeOutput).combinePath("AngelScript/ClientRootModule.fosb");
#elif SINGLE_SCRIPTING
    const string script_out_path = _str(App->Settings.BakeOutput).combinePath("AngelScript/SingleRootModule.fosb");
#elif MAPPER_SCRIPTING
    const string script_out_path = _str(App->Settings.BakeOutput).combinePath("AngelScript/MapperRootModule.fosb");
#endif

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
