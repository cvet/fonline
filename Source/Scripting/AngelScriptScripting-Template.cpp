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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
// ReSharper disable CppJoinDeclarationAndAssignment

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
#include "as_context.h"
#include "datetime/datetime.h"
#include "preprocessor.h"
#include "scriptarray.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "scripthelper/scripthelper.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"
#include "weakref/weakref.h"

// Reset garbage from WinApi
#include "WinApiUndef-Include.h"

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
#define SCRIPTING_CLASS AngelScriptCompiler_ServerScriptSystem
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompiler
#define SCRIPTING_CLASS AngelScriptCompiler_ClientScriptSystem
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompiler
#define SCRIPTING_CLASS AngelScriptCompiler_SingleScriptSystem
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompiler
#define SCRIPTING_CLASS AngelScriptCompiler_MapperScriptSystem
#endif
#else
#if SERVER_SCRIPTING
#define FOEngine AngelScriptServerCompilerValidation
#define SCRIPTING_CLASS AngelScriptCompiler_ServerScriptSystem_Validation
#elif CLIENT_SCRIPTING
#define FOEngine AngelScriptClientCompilerValidation
#define SCRIPTING_CLASS AngelScriptCompiler_ClientScriptSystem_Validation
#elif SINGLE_SCRIPTING
#define FOEngine AngelScriptSingleCompilerValidation
#define SCRIPTING_CLASS AngelScriptCompiler_SingleScriptSystem_Validation
#elif MAPPER_SCRIPTING
#define FOEngine AngelScriptMapperCompilerValidation
#define SCRIPTING_CLASS AngelScriptCompiler_MapperScriptSystem_Validation
#endif
#endif
#endif

#if COMPILER_MODE
class FOServer;
class FOClient;
class FOSingle;
class FOMapper;

class BaseEntity;
class CustomEntity;
class CustomEntityWithProto;
class CustomEntityView;
class CustomEntityWithProtoView;

#if COMPILER_VALIDATION_MODE
#define INIT_ARGS const FileSystem &resources, FOEngineBase **out_engine
#else
#define INIT_ARGS const FileSystem& resources
#endif

struct SCRIPTING_CLASS : public ScriptSystem
{
    void InitAngelScriptScripting(INIT_ARGS);
};

#define ENTITY_VERIFY_NULL(e)
#define ENTITY_VERIFY(e)
#define ENTITY_VERIFY_RETURN(e, ret)

static GlobalSettings DummySettings {};

class FOEngine : public FOEngineBase
{
public:
#if SERVER_SCRIPTING
    FOEngine() :
        FOEngineBase(DummySettings, PropertiesRelationType::ServerRelative)
    {
        STACK_TRACE_ENTRY();

        extern void AngelScript_ServerCompiler_RegisterData(FOEngineBase*);
        AngelScript_ServerCompiler_RegisterData(this);
    }
#elif CLIENT_SCRIPTING
    FOEngine() :
        FOEngineBase(DummySettings, PropertiesRelationType::ClientRelative)
    {
        STACK_TRACE_ENTRY();

        extern void AngelScript_ClientCompiler_RegisterData(FOEngineBase*);
        AngelScript_ClientCompiler_RegisterData(this);
    }
#elif SINGLE_SCRIPTING
    FOEngine() :
        FOEngineBase(DummySettings, PropertiesRelationType::BothRelative)
    {
        STACK_TRACE_ENTRY();

        extern void AngelScript_SingleCompiler_RegisterData(FOEngineBase*);
        AngelScript_SingleCompiler_RegisterData(this);
    }
#elif MAPPER_SCRIPTING
    FOEngine() :
        FOEngineBase(DummySettings, PropertiesRelationType::BothRelative)
    {
        STACK_TRACE_ENTRY();

        extern void AngelScript_MapperCompiler_RegisterData(FOEngineBase*);
        AngelScript_MapperCompiler_RegisterData(this);
    }
#endif
};
#endif

#if !COMPILER_MODE
#define INIT_ARGS

#define GET_AS_ENGINE_FROM_SELF() static_cast<SCRIPTING_CLASS*>(self->GetEngine()->ScriptSys.get())->AngelScriptData->Engine
#define GET_AS_ENGINE_FROM_ENTITY(entity) static_cast<SCRIPTING_CLASS*>(entity->GetEngine()->ScriptSys.get())->AngelScriptData->Engine
#define GET_SCRIPT_SYS_FROM_SELF() static_cast<SCRIPTING_CLASS*>(self->GetEngine()->ScriptSys.get())->AngelScriptData.get()
#define GET_SCRIPT_SYS_FROM_ENTITY(entity) static_cast<SCRIPTING_CLASS*>(entity->GetEngine()->ScriptSys.get())->AngelScriptData.get()

#define ENTITY_VERIFY_NULL(e) \
    if ((e) == nullptr) { \
        throw ScriptException("Access to null entity"); \
    }
#define ENTITY_VERIFY(e) \
    if ((e) != nullptr && (e)->IsDestroyed()) { \
        throw ScriptException("Access to destroyed entity"); \
    }
#define ENTITY_VERIFY_RETURN(e, ret) \
    if ((e) != nullptr && (e)->IsDestroyed()) { \
        return ret; \
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

#define PASS_AS_PVOID(ptr) const_cast<void*>(static_cast<const void*>(ptr))

#if !COMPILER_MODE
#define PTR_OR_DUMMY(ptr) PASS_AS_PVOID(&(ptr))
#else
#define PTR_OR_DUMMY(ptr) PASS_AS_PVOID(&dummy)
#endif

struct AngelscriptAllocator
{
    static auto Alloc(size_t size) -> void*
    {
        constexpr SafeAllocator<uint8> allocator;
        return allocator.allocate(size);
    }

    static void Free(void* ptr)
    {
        constexpr SafeAllocator<uint8> allocator;
        allocator.deallocate(static_cast<uint8*>(ptr), 0);
    }

    AngelscriptAllocator()
    {
        // Setup AS allocators on startup
        asSetGlobalMemoryFunctions(&AngelscriptAllocator::Alloc, &AngelscriptAllocator::Free);
    }
} static AngelscriptAllocator_;

#if !COMPILER_MODE
#define GET_CONTEXT_EXT(ctx) *static_cast<asCContext*>(ctx)->Ext
#define GET_CONTEXT_EXT2(ctx) static_cast<asCContext*>(ctx)->Ext2

struct StackTraceEntryStorage
{
    static constexpr size_t STACK_TRACE_FUNC_BUF_SIZE = 64;
    static constexpr size_t STACK_TRACE_FILE_BUF_SIZE = 128;

    std::array<char, STACK_TRACE_FUNC_BUF_SIZE> FuncBuf {};
    size_t FuncBufLen {};
    std::array<char, STACK_TRACE_FILE_BUF_SIZE> FileBuf {};
    size_t FileBufLen {};

    SourceLocationData SrcLoc {};
};

struct ASContextExtendedData
{
    bool ExecutionActive {};
    size_t ExecutionCalls {};
    unordered_map<size_t, StackTraceEntryStorage>* ScriptCallCacheEntries {};
    std::string Info {};
    asIScriptContext* Parent {};
    time_point SuspendEndTime {};
    Entity* ValidCheck {};
#if FO_TRACY
    vector<TracyCZoneCtx> TracyExecutionCalls {};
#endif
};

static void AngelScriptBeginCall(asIScriptContext* ctx, asIScriptFunction* func, size_t program_pos);
static void AngelScriptEndCall(asIScriptContext* ctx);
static void AngelScriptException(asIScriptContext* ctx, void* param);

struct SCRIPTING_CLASS::AngelScriptImpl
{
    struct EnumInfo
    {
        string EnumName {};
        FOEngine* GameEngine {};
    };

    void CreateContext()
    {
        STACK_TRACE_ENTRY();

        auto* ctx = Engine->CreateContext();
        RUNTIME_ASSERT(ctx);

        static_cast<asCContext*>(ctx)->Ext = SafeAlloc::MakeRaw<ASContextExtendedData>();
        auto& ctx_ext = GET_CONTEXT_EXT(ctx);
        ctx_ext.ScriptCallCacheEntries = &ScriptCallCacheEntries;
#if FO_TRACY
        ctx_ext.TracyExecutionCalls.reserve(64);
#endif

        auto& ctx_ext2 = GET_CONTEXT_EXT2(ctx);
#if !FO_NO_MANUAL_STACK_TRACE
        ctx_ext2.BeginScriptCall = AngelScriptBeginCall;
        ctx_ext2.EndScriptCall = AngelScriptEndCall;
#endif

        int r = ctx->SetExceptionCallback(asFUNCTION(AngelScriptException), &ExceptionStackTrace, asCALL_CDECL);
        RUNTIME_ASSERT(r >= 0);

        FreeContexts.push_back(ctx);
    }

    void FinishContext(asIScriptContext* ctx)
    {
        STACK_TRACE_ENTRY();

        auto it = std::find(FreeContexts.begin(), FreeContexts.end(), ctx);
        RUNTIME_ASSERT(it != FreeContexts.end());
        FreeContexts.erase(it);

        delete static_cast<asCContext*>(ctx)->Ext;
        static_cast<asCContext*>(ctx)->Ext = nullptr;
        ctx->Release();
    }

    auto RequestContext() -> asIScriptContext*
    {
        STACK_TRACE_ENTRY();

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
        STACK_TRACE_ENTRY();

        ctx->Unprepare();

        auto it = std::find(BusyContexts.begin(), BusyContexts.end(), ctx);
        RUNTIME_ASSERT(it != BusyContexts.end());
        BusyContexts.erase(it);
        FreeContexts.push_back(ctx);

        auto& ctx_ext = GET_CONTEXT_EXT(ctx);
        ctx_ext.Parent = nullptr;
        ctx_ext.Info.clear();
        ctx_ext.SuspendEndTime = {};
        ctx_ext.ValidCheck = {};
    }

    auto PrepareContext(asIScriptFunction* func) -> asIScriptContext*
    {
        STACK_TRACE_ENTRY();

        RUNTIME_ASSERT(func);

        auto* ctx = RequestContext();
        const auto as_result = ctx->Prepare(func);

        if (as_result < 0) {
            ReturnContext(ctx);

            throw ScriptCallException("Can't prepare context", func->GetName(), as_result);
        }

        return ctx;
    }

    auto RunContext(asIScriptContext* ctx, bool can_suspend) -> bool
    {
        STACK_TRACE_ENTRY();

        auto& ctx_ext = GET_CONTEXT_EXT(ctx);
        ctx_ext.Parent = asGetActiveContext();

        int exec_result = 0;

        {
            ctx_ext.ExecutionActive = true;
            ctx_ext.ExecutionCalls = 0;
#if FO_TRACY
            ctx_ext.TracyExecutionCalls.clear();
#endif

            auto after_execution = ScopeCallback([&]() noexcept {
                ctx_ext.ExecutionActive = false;
                while (ctx_ext.ExecutionCalls > 0) {
                    ctx_ext.ExecutionCalls--;
                    PopStackTrace();
                }
            });

            const auto is_suspended_execution = ctx->GetState() == asEXECUTION_SUSPENDED;
            const auto execution_time = TimeMeter();

            exec_result = ctx->Execute();

            const auto execution_duration = execution_time.GetDuration();

            if (execution_duration >= std::chrono::milliseconds {GameEngine->Settings.ScriptOverrunReportTime} && !IsRunInDebugger()) {
#if !FO_DEBUG
                WriteLog("Script execution overrun: {} ({}{})", ctx->GetFunction()->GetDeclaration(true, true), execution_duration, is_suspended_execution ? ", was suspended at start" : "");
#else
                UNUSED_VARIABLE(is_suspended_execution);
#endif
            }

#if FO_TRACY
            while (!ctx_ext.TracyExecutionCalls.empty()) {
                ___tracy_emit_zone_end(ctx_ext.TracyExecutionCalls.back());
                ctx_ext.TracyExecutionCalls.pop_back();
            }
#endif
        }

        if (exec_result == asEXECUTION_SUSPENDED) {
            if (!can_suspend) {
                ctx->Abort();
                ReturnContext(ctx);

                throw ScriptCallException("Can't yield current routine");
            }

            return false;
        }

        if (exec_result != asEXECUTION_FINISHED) {
            if (exec_result == asEXECUTION_EXCEPTION) {
                const string ex_string = ctx->GetExceptionString();

                ctx->Abort();
                ReturnContext(ctx);

                throw ScriptCallException(ExceptionStackTraceData {ExceptionStackTrace}, ex_string);
            }

            if (exec_result == asEXECUTION_ABORTED) {
                ReturnContext(ctx);

                throw ScriptCallException("Execution of script aborted");
            }

            ctx->Abort();
            ReturnContext(ctx);

            throw ScriptCallException("Context execution error", exec_result);
        }

        return true;
    }

    void ResumeSuspendedContexts()
    {
        STACK_TRACE_ENTRY();

        if (BusyContexts.empty()) {
            return;
        }

        vector<asIScriptContext*> resume_contexts;
        vector<asIScriptContext*> finish_contexts;
        const auto time = GameEngine->GameTime.FrameTime();

        for (auto* ctx : BusyContexts) {
            const auto& ctx_ext = GET_CONTEXT_EXT(ctx);

            if ((ctx->GetState() == asEXECUTION_PREPARED || ctx->GetState() == asEXECUTION_SUSPENDED) && time >= ctx_ext.SuspendEndTime) {
                if (ctx_ext.ValidCheck != nullptr && ctx_ext.ValidCheck->IsDestroyed()) {
                    finish_contexts.emplace_back(ctx);
                }
                else {
                    resume_contexts.emplace_back(ctx);
                }
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

        for (auto* ctx : finish_contexts) {
            ReturnContext(ctx);
        }
    }

    FOEngine* GameEngine {};
    asIScriptEngine* Engine {};
    set<CScriptArray*> EnumArrays {};
    map<string, EnumInfo> EnumInfos {};
    set<hstring> ContentData {};
    asIScriptContext* CurrentCtx {};
    vector<asIScriptContext*> FreeContexts {};
    vector<asIScriptContext*> BusyContexts {};
    string ExceptionStackTrace {};
    unordered_map<size_t, StackTraceEntryStorage> ScriptCallCacheEntries {};
    unordered_set<hstring> HashedStrings {};
    unordered_map<asIScriptFunction*, ScriptFuncDesc> FuncMap {};
};

static void AngelScriptBeginCall(asIScriptContext* ctx, asIScriptFunction* func, size_t program_pos)
{
    auto& ctx_ext = GET_CONTEXT_EXT(ctx);

    if (!ctx_ext.ExecutionActive) {
        return;
    }

    const auto it = ctx_ext.ScriptCallCacheEntries->find(program_pos);

    if (it == ctx_ext.ScriptCallCacheEntries->end()) {
        int ctx_line = ctx->GetLineNumber();

        auto* lnt = static_cast<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
        const auto& orig_file = Preprocessor::ResolveOriginalFile(ctx_line, lnt);
        const auto orig_line = Preprocessor::ResolveOriginalLine(ctx_line, lnt);

        const auto* func_decl = func->GetDeclaration(true);

        const auto safe_copy = [](auto& to, size_t& len, string_view from) {
            len = std::min(from.length(), to.size() - 1);
            MemCopy(to.data(), from.data(), len);
            to[len] = 0;
        };

        auto& storage = ctx_ext.ScriptCallCacheEntries->emplace(program_pos, StackTraceEntryStorage {}).first->second;

        safe_copy(storage.FuncBuf, storage.FuncBufLen, func_decl);
        safe_copy(storage.FileBuf, storage.FileBufLen, orig_file);

        storage.SrcLoc.name = nullptr;
        storage.SrcLoc.function = storage.FuncBuf.data();
        storage.SrcLoc.file = storage.FileBuf.data();
        storage.SrcLoc.line = static_cast<uint32_t>(orig_line);

        PushStackTrace(storage.SrcLoc);

#if FO_TRACY
        const auto tracy_srcloc = ___tracy_alloc_srcloc(storage.SrcLoc.line, storage.FileBuf.data(), storage.FileBufLen, storage.FuncBuf.data(), storage.FuncBufLen, 0);
        const auto tracy_ctx = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
        ctx_ext.TracyExecutionCalls.emplace_back(tracy_ctx);
#endif
    }
    else {
        const auto& storage = it->second;

        PushStackTrace(storage.SrcLoc);

#if FO_TRACY
        const auto tracy_srcloc = ___tracy_alloc_srcloc(storage.SrcLoc.line, storage.FileBuf.data(), storage.FileBufLen, storage.FuncBuf.data(), storage.FuncBufLen, 0);
        const auto tracy_ctx = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
        ctx_ext.TracyExecutionCalls.emplace_back(tracy_ctx);
#endif
    }

    ctx_ext.ExecutionCalls++;
}

static void AngelScriptEndCall(asIScriptContext* ctx)
{
    auto& ctx_ext = GET_CONTEXT_EXT(ctx);

    if (!ctx_ext.ExecutionActive) {
        return;
    }

    if (ctx_ext.ExecutionCalls > 0) {
        PopStackTrace();

        ctx_ext.ExecutionCalls--;

#if FO_TRACY
        ___tracy_emit_zone_end(ctx_ext.TracyExecutionCalls.back());
        ctx_ext.TracyExecutionCalls.pop_back();
#endif
    }
}

static void AngelScriptException(asIScriptContext* ctx, void* param)
{
    auto* ex_func = ctx->GetExceptionFunction();
    const auto ex_line = ctx->GetExceptionLineNumber();

    auto* lnt = static_cast<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
    const auto& ex_orig_file = Preprocessor::ResolveOriginalFile(ex_line, lnt);
    const auto ex_orig_line = Preprocessor::ResolveOriginalLine(ex_line, lnt);

    const auto* func_decl = ex_func->GetDeclaration(true);

    {
        auto srcloc = SourceLocationData {nullptr, func_decl, ex_orig_file.c_str(), static_cast<uint32_t>(ex_orig_line)};
        PushStackTrace(srcloc);
        auto stack_trace_entry_end = ScopeCallback([]() noexcept { PopStackTrace(); });

        // Write to ExceptionStackTrace
        *static_cast<string*>(param) = GetStackTrace();
    }
}
#endif

template<typename T>
static T* ScriptableObject_Factory()
{
    STACK_TRACE_ENTRY();

    return SafeAlloc::MakeRaw<T>();
}

#if !COMPILER_MODE
// Arrays stuff
[[maybe_unused]] static auto CreateASArray(asIScriptEngine* as_engine, const char* type) -> CScriptArray*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* as_type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(as_type_info);
    auto* as_array = CScriptArray::Create(as_type_info);
    RUNTIME_ASSERT(as_array);
    return as_array;
}

template<typename T, typename T2 = T>
[[maybe_unused]] static auto MarshalArray(asIScriptEngine* as_engine, CScriptArray* as_array) -> vector<T>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(as_engine);

    if (as_array == nullptr || as_array->GetSize() == 0) {
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
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(as_engine);

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
    STACK_TRACE_ENTRY();

    auto* as_array = CreateASArray(as_engine, type);

    if (!vec.empty()) {
        AssignArray<T, T2>(as_engine, vec, as_array);
    }

    return as_array;
}

[[maybe_unused]] static auto CreateASDict(asIScriptEngine* as_engine, const char* type) -> CScriptDict*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(as_engine);
    const auto type_id = as_engine->GetTypeIdByDecl(type);
    RUNTIME_ASSERT(type_id);
    auto* as_type_info = as_engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(as_type_info);
    auto* as_dict = CScriptDict::Create(as_type_info);
    RUNTIME_ASSERT(as_dict);
    return as_dict;
}

template<typename T, typename U, typename T2 = T, typename U2 = U>
[[maybe_unused]] static auto MarshalDict(asIScriptEngine* as_engine, CScriptDict* as_dict) -> map<T, U>
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(as_engine);

    static_assert(std::is_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_same_v<T, any_t> || is_strong_type_v<T> || is_valid_pod_type_v<T>);
    static_assert(std::is_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_same_v<U, any_t> || is_strong_type_v<U> || is_valid_pod_type_v<U>);

    if (as_dict == nullptr || as_dict->GetSize() == 0) {
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
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(as_engine);

    static_assert(std::is_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_same_v<T, any_t> || is_strong_type_v<T> || is_valid_pod_type_v<T>);
    static_assert(std::is_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_same_v<U, any_t> || is_strong_type_v<U> || is_valid_pod_type_v<U>);

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
    STACK_TRACE_ENTRY();

    static_assert(std::is_enum_v<T> || std::is_arithmetic_v<T> || std::is_same_v<T, string> || std::is_same_v<T, hstring> || std::is_same_v<T, any_t> || is_strong_type_v<T> || is_valid_pod_type_v<T>);
    static_assert(std::is_enum_v<U> || std::is_arithmetic_v<U> || std::is_same_v<U, string> || std::is_same_v<U, hstring> || std::is_same_v<U, any_t> || is_strong_type_v<U> || is_valid_pod_type_v<U>);

    auto* as_dict = CreateASDict(as_engine, type);

    if (!map.empty()) {
        AssignDict<T, U, T2, U2>(as_engine, map, as_dict);
    }

    return as_dict;
}

[[maybe_unused]] static auto GetASObjectInfo(const void* ptr, int type_id) -> string
{
    STACK_TRACE_ENTRY();

    switch (type_id) {
    case asTYPEID_VOID:
        return "";
    case asTYPEID_BOOL:
        return strex("{}", *static_cast<const bool*>(ptr) ? "true" : "false");
    case asTYPEID_INT8:
        return strex("{}", *static_cast<const int8*>(ptr));
    case asTYPEID_INT16:
        return strex("{}", *static_cast<const int16*>(ptr));
    case asTYPEID_INT32:
        return strex("{}", *static_cast<const int*>(ptr));
    case asTYPEID_INT64:
        return strex("{}", *static_cast<const int64*>(ptr));
    case asTYPEID_UINT8:
        return strex("{}", *static_cast<const uint8*>(ptr));
    case asTYPEID_UINT16:
        return strex("{}", *static_cast<const uint16*>(ptr));
    case asTYPEID_UINT32:
        return strex("{}", *static_cast<const uint*>(ptr));
    case asTYPEID_UINT64:
        return strex("{}", *static_cast<const uint64*>(ptr));
    case asTYPEID_FLOAT:
        return strex("{}", *static_cast<const float*>(ptr));
    case asTYPEID_DOUBLE:
        return strex("{}", *static_cast<const double*>(ptr));
    default:
        break;
    }

    // Todo: GetASObjectInfo add detailed info about object
    auto* ctx = asGetActiveContext();
    RUNTIME_ASSERT(ctx);
    auto* engine = ctx->GetEngine();
    auto* as_type_info = engine->GetTypeInfoById(type_id);
    RUNTIME_ASSERT(as_type_info);
    const string type_name = as_type_info->GetName();

    if (type_name == "string") {
        return strex("{}", *static_cast<const string*>(ptr));
    }
    if (type_name == "hstring") {
        return strex("{}", *static_cast<const hstring*>(ptr));
    }
    if (type_name == "any") {
        return strex("{}", *static_cast<const any_t*>(ptr));
    }
    if (type_name == IDENT_T_NAME) {
        return strex("{}", *static_cast<const ident_t*>(ptr));
    }
    if (type_name == TICK_T_NAME) {
        return strex("{}", *static_cast<const tick_t*>(ptr));
    }

    return strex("{}", type_name);
}
#endif

[[maybe_unused]] static auto GetASFuncName(const asIScriptFunction* func, HashResolver& hash_resolver) -> hstring
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(func);

    if (func->GetDelegateObject() != nullptr) {
        throw ScriptException("Function can't be delegate", func->GetDelegateFunction()->GetName());
    }

    string func_name;

    if (func->GetNamespace() == nullptr) {
        func_name = strex("{}", func->GetName()).str();
    }
    else {
        func_name = strex("{}::{}", func->GetNamespace(), func->GetName()).str();
    }

    return hash_resolver.ToHashedString(func_name);
}

#if !COMPILER_MODE
static auto AngelScriptFuncCall(SCRIPTING_CLASS::AngelScriptImpl* script_sys, ScriptFuncDesc* func_desc, asIScriptFunction* func, initializer_list<void*> args, void* ret) noexcept -> bool
{
    STACK_TRACE_ENTRY();

    try {
        RUNTIME_ASSERT(func);
        RUNTIME_ASSERT(func_desc);
        RUNTIME_ASSERT(func_desc->CallSupported);
        RUNTIME_ASSERT(func_desc->ArgsType.size() == args.size());
        RUNTIME_ASSERT(func_desc->ArgsType.size() == func->GetParamCount());

        if (ret != nullptr) {
            RUNTIME_ASSERT(func->GetReturnTypeId() != asTYPEID_VOID);
            RUNTIME_ASSERT(func_desc->RetType->Accessor);
        }
        else {
            RUNTIME_ASSERT(func->GetReturnTypeId() == asTYPEID_VOID);
            RUNTIME_ASSERT(!func_desc->RetType->Accessor);
        }

        auto* ctx = script_sys->PrepareContext(func);

        if (args.size() != 0) {
            auto it = args.begin();

            for (asUINT i = 0; i < args.size(); i++, it++) {
                auto* arg = *it;
                auto& arg_type = func_desc->ArgsType[i];

                if (arg_type->Accessor->IsArray()) {
                    CScriptArray* arr = CreateASArray(ctx->GetEngine(), strex("{}[]", arg_type->Name).c_str());

                    for (size_t n = 0, m = arg_type->Accessor->GetArraySize(arg); n < m; n++) {
                        auto* elem = arg_type->Accessor->GetArrayElement(arg, n);
                        arr->InsertLast(elem);
                    }

                    ctx->SetArgObject(i, arr);
                    arr->Release();
                }
                else if (arg_type->Accessor->IsReference()) {
                    ctx->SetArgAddress(i, arg_type->Accessor->GetData(arg));
                }
                else if (arg_type->Accessor->IsObject() || arg_type->Accessor->IsEntity()) {
                    ctx->SetArgObject(i, arg_type->Accessor->GetData(arg));
                }
                else if (arg_type->Accessor->IsPlainData()) {
                    MemCopy(ctx->GetAddressOfArg(i), arg_type->Accessor->GetData(arg), arg_type->Accessor->GetDataSize());
                }
                else {
                    UNREACHABLE_PLACE();
                }
            }
        }

        if (script_sys->RunContext(ctx, ret == nullptr)) {
            if (ret != nullptr) {
                RUNTIME_ASSERT(func_desc->RetType->Accessor->IsPlainData());
                MemCopy(ret, ctx->GetAddressOfReturnValue(), func_desc->RetType->Accessor->GetDataSize());
            }

            script_sys->ReturnContext(ctx);
        }

        return true;
    }
    catch (std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }

    return false;
}

template<typename TRet, typename... Args>
[[maybe_unused]] static auto GetASScriptFunc(asIScriptFunction* func, SCRIPTING_CLASS::AngelScriptImpl* script_sys) -> ScriptFunc<TRet, Args...>
{
    STACK_TRACE_ENTRY();

    const auto it = script_sys->FuncMap.find(func);

    if (it == script_sys->FuncMap.end()) {
        const auto [it_inserted, inserted] = script_sys->FuncMap.emplace(func, ScriptFuncDesc());
        RUNTIME_ASSERT(inserted);

        auto& func_desc = it_inserted->second;
        func_desc.Name = func->GetDelegateObject() == nullptr ? GetASFuncName(func, *script_sys->GameEngine) : hstring();
        func_desc.CallSupported = true;
        func_desc.RetType = script_sys->GameEngine->ScriptSys->ResolveScriptType(typeid(TRet));
        func_desc.ArgsType = {script_sys->GameEngine->ScriptSys->ResolveScriptType(typeid(Args))...};
        func_desc.Delegate = func->GetDelegateObject() != nullptr;
        func_desc.Call = [script_sys, &func_desc, as_func = RefCountHolder(func)](initializer_list<void*> args, void* ret) noexcept { //
            return AngelScriptFuncCall(script_sys, &func_desc, as_func.get(), args, ret);
        };

        return ScriptFunc<TRet, Args...>(&func_desc);
    }
    else {
        return ScriptFunc<TRet, Args...>(&it->second);
    }
}

template<typename TRet, typename... Args>
[[maybe_unused]] static auto GetASScriptFuncName(asIScriptFunction* func, SCRIPTING_CLASS::AngelScriptImpl* script_sys) -> ScriptFuncName<TRet, Args...>
{
    STACK_TRACE_ENTRY();

    if (func->GetDelegateObject() != nullptr) {
        throw ScriptException("Function must be global (not delegate)");
    }

    return GetASFuncName(func, *script_sys->GameEngine);
}
#endif

#if !COMPILER_MODE
static auto CalcConstructAddrSpace(const Property* prop) -> size_t
{
    STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            return sizeof(hstring);
        }
        else if (prop->IsBaseTypeEnum()) {
            return sizeof(int);
        }
        else if (prop->IsBaseTypePrimitive()) {
            return prop->GetBaseSize();
        }
        else if (prop->IsBaseTypeStruct()) {
            return prop->GetBaseSize();
        }
        else {
            UNREACHABLE_PLACE();
        }
    }
    else if (prop->IsString()) {
        return sizeof(string);
    }
    else if (prop->IsArray()) {
        return sizeof(CScriptArray*);
    }
    else if (prop->IsDict()) {
        return sizeof(CScriptDict*);
    }
    else {
        UNREACHABLE_PLACE();
    }
}

static void FreeConstructAddrSpace(const Property* prop, void* construct_addr)
{
    STACK_TRACE_ENTRY();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            static_cast<hstring*>(construct_addr)->~hstring();
        }
    }
    else if (prop->IsString()) {
        static_cast<string*>(construct_addr)->~string();
    }
    else if (prop->IsArray()) {
        (*static_cast<CScriptArray**>(construct_addr))->Release();
    }
    else if (prop->IsDict()) {
        (*static_cast<CScriptDict**>(construct_addr))->Release();
    }
    else {
        UNREACHABLE_PLACE();
    }
}

static void PropsToAS(const Property* prop, PropertyRawData& prop_data, void* construct_addr, asIScriptEngine* as_engine)
{
    STACK_TRACE_ENTRY();

    const auto resolve_hash = [prop](const void* hptr) -> hstring {
        const auto hash = *reinterpret_cast<const hstring::hash_t*>(hptr);
        const auto& hash_resolver = prop->GetRegistrator()->GetHashResolver();
        return hash_resolver.ResolveHash(hash);
    };
    const auto resolve_enum = [](const void* eptr, size_t elen) -> int {
        int result = 0;
        MemCopy(&result, eptr, elen);
        return result;
    };

    const auto* data = prop_data.GetPtrAs<uint8>();
    const auto data_size = prop_data.GetSize();

    if (prop->IsPlainData()) {
        if (prop->IsBaseTypeHash()) {
            RUNTIME_ASSERT(data_size == sizeof(hstring::hash_t));
            new (construct_addr) hstring(resolve_hash(data));
        }
        else if (prop->IsBaseTypeEnum()) {
            RUNTIME_ASSERT(data_size != 0);
            RUNTIME_ASSERT(data_size <= sizeof(int));
            MemFill(construct_addr, 0, sizeof(int));
            MemCopy(construct_addr, data, data_size);
        }
        else if (prop->IsBaseTypePrimitive()) {
            RUNTIME_ASSERT(data_size != 0);
            MemCopy(construct_addr, data, data_size);
        }
        else if (prop->IsBaseTypeStruct()) {
            RUNTIME_ASSERT(data_size != 0);
            MemCopy(construct_addr, data, data_size);
        }
        else {
            UNREACHABLE_PLACE();
        }
    }
    else if (prop->IsString()) {
        new (construct_addr) string(reinterpret_cast<const char*>(data), data_size);
    }
    else if (prop->IsArray()) {
        auto* arr = CreateASArray(as_engine, prop->GetFullTypeName().c_str());

        if (prop->IsArrayOfString()) {
            if (data_size != 0) {
                uint arr_size;
                MemCopy(&arr_size, data, sizeof(arr_size));
                data += sizeof(arr_size);

                arr->Resize(arr_size);

                for (uint i = 0; i < arr_size; i++) {
                    uint str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(str_size);

                    string str(reinterpret_cast<const char*>(data), str_size);
                    arr->SetValue(i, &str);

                    data += str_size;
                }
            }
        }
        else if (prop->IsBaseTypeHash()) {
            if (data_size != 0) {
                const auto arr_size = static_cast<uint>(data_size / prop->GetBaseSize());
                arr->Resize(arr_size);

                for (uint i = 0; i < arr_size; i++) {
                    const auto hvalue = resolve_hash(data);
                    arr->SetValue(i, PASS_AS_PVOID(&hvalue));

                    data += sizeof(hstring::hash_t);
                }
            }
        }
        else if (prop->IsBaseTypeEnum()) {
            if (data_size != 0) {
                const auto arr_size = static_cast<uint>(data_size / prop->GetBaseSize());
                arr->Resize(arr_size);

                if (prop->GetBaseSize() == sizeof(int)) {
                    MemCopy(arr->At(0), data, data_size);
                }
                else {
                    auto* dest = static_cast<int*>(arr->At(0));

                    for (uint i = 0; i < arr_size; i++) {
                        *(dest + i) = 0;
                        MemCopy(dest + i, data + i * prop->GetBaseSize(), prop->GetBaseSize());
                    }
                }
            }
        }
        else if (prop->IsBaseTypePrimitive()) {
            if (data_size != 0) {
                const auto arr_size = static_cast<uint>(data_size / prop->GetBaseSize());
                arr->Resize(arr_size);
                MemCopy(arr->At(0), data, data_size);
            }
        }
        else if (prop->IsBaseTypeStruct()) {
            if (data_size != 0) {
                const auto arr_size = static_cast<uint>(data_size / prop->GetBaseSize());
                arr->Resize(arr_size);

                for (uint i = 0; i < arr_size; i++) {
                    MemCopy(arr->At(i), data, prop->GetBaseSize());
                    data += prop->GetBaseSize();
                }
            }
        }
        else {
            UNREACHABLE_PLACE();
        }

        *static_cast<CScriptArray**>(construct_addr) = arr;
    }
    else if (prop->IsDict()) {
        CScriptDict* dict = CreateASDict(as_engine, prop->GetFullTypeName().c_str());

        if (data_size != 0) {
            if (prop->IsDictOfArray()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeySize();
                    }

                    uint arr_size;
                    MemCopy(&arr_size, data, sizeof(arr_size));
                    data += sizeof(arr_size);

                    auto* arr = CreateASArray(as_engine, strex("{}[]", prop->GetBaseTypeName()).c_str());

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            arr->Resize(arr_size);

                            for (uint i = 0; i < arr_size; i++) {
                                uint str_size;
                                MemCopy(&str_size, data, sizeof(str_size));
                                data += sizeof(str_size);

                                string str(reinterpret_cast<const char*>(data), str_size);
                                arr->SetValue(i, &str);
                                data += str_size;
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            arr->Resize(arr_size);

                            for (uint i = 0; i < arr_size; i++) {
                                const auto hvalue = resolve_hash(data);
                                arr->SetValue(i, PASS_AS_PVOID(&hvalue));

                                data += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            arr->Resize(arr_size);

                            if (prop->GetBaseSize() == 4) {
                                MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            }
                            else {
                                auto* dest = static_cast<int*>(arr->At(0));
                                for (uint i = 0; i < arr_size; i++) {
                                    MemCopy(dest + i, data + i * prop->GetBaseSize(), prop->GetBaseSize());
                                }
                            }

                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            arr->Resize(arr_size);

                            MemCopy(arr->At(0), data, arr_size * prop->GetBaseSize());
                            data += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            arr->Resize(arr_size);

                            for (uint i = 0; i < arr_size; i++) {
                                MemCopy(arr->At(i), data, prop->GetBaseSize());
                                data += prop->GetBaseSize();
                            }
                        }
                        else {
                            UNREACHABLE_PLACE();
                        }
                    }

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};
                        dict->Set(PASS_AS_PVOID(&key_str), &arr);
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(PASS_AS_PVOID(&hkey), &arr);
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeySize());
                        dict->Set(PASS_AS_PVOID(&ekey), &arr);
                    }
                    else {
                        dict->Set(PASS_AS_PVOID(key), &arr);
                    }

                    arr->Release();
                }
            }
            else if (prop->IsDictOfString()) {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeySize();
                    }

                    uint str_size;
                    MemCopy(&str_size, data, sizeof(str_size));
                    data += sizeof(uint);

                    auto str = string {reinterpret_cast<const char*>(data), str_size};

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};
                        dict->Set(PASS_AS_PVOID(&key_str), &str);
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);
                        dict->Set(PASS_AS_PVOID(&hkey), &str);
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeySize());
                        dict->Set(PASS_AS_PVOID(&ekey), &str);
                    }
                    else {
                        dict->Set(PASS_AS_PVOID(key), &str);
                    }

                    data += str_size;
                }
            }
            else {
                const auto* data_end = data + data_size;

                while (data < data_end) {
                    const auto* key = data;

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        data += sizeof(key_len) + key_len;
                    }
                    else {
                        data += prop->GetDictKeySize();
                    }

                    if (prop->IsDictKeyString()) {
                        const uint key_len = *reinterpret_cast<const uint*>(key);
                        const auto key_str = string {reinterpret_cast<const char*>(key + sizeof(key_len)), key_len};

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(PASS_AS_PVOID(&key_str), PASS_AS_PVOID(&hvalue));
                        }
                        else {
                            dict->Set(PASS_AS_PVOID(&key_str), PASS_AS_PVOID(data));
                        }
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = resolve_hash(key);

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(PASS_AS_PVOID(&hkey), PASS_AS_PVOID(&hvalue));
                        }
                        else {
                            dict->Set(PASS_AS_PVOID(&hkey), PASS_AS_PVOID(data));
                        }
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = resolve_enum(key, prop->GetDictKeySize());

                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(PASS_AS_PVOID(&ekey), PASS_AS_PVOID(&hvalue));
                        }
                        else {
                            dict->Set(PASS_AS_PVOID(&ekey), PASS_AS_PVOID(data));
                        }
                    }
                    else {
                        if (prop->IsBaseTypeHash()) {
                            const auto hvalue = resolve_hash(data);
                            dict->Set(PASS_AS_PVOID(key), PASS_AS_PVOID(&hvalue));
                        }
                        else {
                            dict->Set(PASS_AS_PVOID(key), PASS_AS_PVOID(data));
                        }
                    }

                    data += prop->GetBaseSize();
                }
            }
        }

        *static_cast<CScriptDict**>(construct_addr) = dict;
    }
    else {
        UNREACHABLE_PLACE();
    }
}

static auto ASToProps(const Property* prop, void* as_obj) -> PropertyRawData
{
    STACK_TRACE_ENTRY();

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
            const auto arr_size = (arr != nullptr ? arr->GetSize() : 0);

            if (arr_size != 0) {
                // Calculate size
                size_t data_size = sizeof(uint);

                for (uint i = 0; i < arr_size; i++) {
                    const auto& str = *static_cast<const string*>(arr->At(i));
                    data_size += sizeof(uint) + static_cast<uint>(str.length());
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                MemCopy(buf, &arr_size, sizeof(arr_size));
                buf += sizeof(uint);

                for (uint i = 0; i < arr_size; i++) {
                    const auto& str = *static_cast<const string*>(arr->At(i));

                    uint str_size = static_cast<uint>(str.length());
                    MemCopy(buf, &str_size, sizeof(str_size));
                    buf += sizeof(str_size);

                    if (str_size != 0) {
                        MemCopy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else {
            const uint arr_size = (arr != nullptr ? arr->GetSize() : 0);
            const size_t data_size = (arr != nullptr ? arr_size * prop->GetBaseSize() : 0);

            if (data_size != 0) {
                if (prop->IsBaseTypeHash()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint i = 0; i < arr_size; i++) {
                        const auto hash = static_cast<const hstring*>(arr->At(i))->as_hash();
                        MemCopy(buf, &hash, sizeof(hstring::hash_t));
                        buf += prop->GetBaseSize();
                    }
                }
                else if (prop->IsBaseTypeEnum()) {
                    if (prop->GetBaseSize() == sizeof(int)) {
                        prop_data.Pass(arr->At(0), data_size);
                    }
                    else {
                        auto* buf = prop_data.Alloc(data_size);

                        for (uint i = 0; i < arr_size; i++) {
                            const auto e = *static_cast<const int*>(arr->At(i));
                            MemCopy(buf, &e, prop->GetBaseSize());
                            buf += prop->GetBaseSize();
                        }
                    }
                }
                else if (prop->IsBaseTypePrimitive()) {
                    prop_data.Pass(arr->At(0), data_size);
                }
                else if (prop->IsBaseTypeStruct()) {
                    auto* buf = prop_data.Alloc(data_size);

                    for (uint i = 0; i < arr_size; i++) {
                        MemCopy(buf, arr->At(i), prop->GetBaseSize());
                        buf += prop->GetBaseSize();
                    }
                }
                else {
                    UNREACHABLE_PLACE();
                }
            }
        }
    }
    else if (prop->IsDict()) {
        const auto* dict = *static_cast<CScriptDict**>(as_obj);

        if (prop->IsDictOfArray()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;
                std::vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *static_cast<const string*>(key);
                        const uint key_len = static_cast<uint>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeySize();
                    }

                    const auto* arr = *static_cast<const CScriptArray**>(value);
                    const uint arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    data_size += sizeof(arr_size);

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

                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *static_cast<const string*>(key);
                        const uint key_len = static_cast<uint>(key_str.length());
                        MemCopy(buf, &key_len, sizeof(key_len));
                        buf += sizeof(key_len);
                        MemCopy(buf, key_str.c_str(), key_len);
                        buf += key_len;
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = static_cast<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *static_cast<const int*>(key);
                        MemCopy(buf, &ekey, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }

                    const uint arr_size = (arr != nullptr ? arr->GetSize() : 0);
                    MemCopy(buf, &arr_size, sizeof(uint));
                    buf += sizeof(arr_size);

                    if (arr_size != 0) {
                        if (prop->IsDictOfArrayOfString()) {
                            for (uint i = 0; i < arr_size; i++) {
                                const auto& str = *static_cast<const string*>(arr->At(i));
                                const auto str_size = static_cast<uint>(str.length());

                                MemCopy(buf, &str_size, sizeof(uint));
                                buf += sizeof(str_size);

                                if (str_size != 0) {
                                    MemCopy(buf, str.c_str(), str_size);
                                    buf += str_size;
                                }
                            }
                        }
                        else if (prop->IsBaseTypeHash()) {
                            for (uint i = 0; i < arr_size; i++) {
                                const auto hash = static_cast<const hstring*>(arr->At(i))->as_hash();
                                MemCopy(buf, &hash, sizeof(hstring::hash_t));
                                buf += sizeof(hstring::hash_t);
                            }
                        }
                        else if (prop->IsBaseTypeEnum()) {
                            if (prop->GetBaseSize() == sizeof(int)) {
                                MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                                buf += arr_size * prop->GetBaseSize();
                            }
                            else {
                                for (uint i = 0; i < arr_size; i++) {
                                    const auto e = *static_cast<const int*>(arr->At(i));
                                    MemCopy(buf, &e, prop->GetBaseSize());
                                    buf += prop->GetBaseSize();
                                }
                            }
                        }
                        else if (prop->IsBaseTypePrimitive()) {
                            MemCopy(buf, arr->At(0), arr_size * prop->GetBaseSize());
                            buf += arr_size * prop->GetBaseSize();
                        }
                        else if (prop->IsBaseTypeStruct()) {
                            for (uint i = 0; i < arr_size; i++) {
                                MemCopy(buf, arr->At(i), prop->GetBaseSize());
                                buf += prop->GetBaseSize();
                            }
                        }
                        else {
                            UNREACHABLE_PLACE();
                        }
                    }
                }
            }
        }
        else if (prop->IsDictOfString()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;
                std::vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *static_cast<const string*>(key);
                        const uint key_len = static_cast<uint>(key_str.length());
                        data_size += sizeof(key_len) + key_len;
                    }
                    else {
                        data_size += prop->GetDictKeySize();
                    }

                    const auto& str = *static_cast<const string*>(value);
                    const auto str_size = static_cast<uint>(str.length());

                    data_size += sizeof(str_size) + str_size;
                }

                // Make buffer
                uint8* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict_map) {
                    const auto& str = *static_cast<const string*>(value);

                    if (prop->IsDictKeyString()) {
                        const auto& key_str = *static_cast<const string*>(key);
                        const uint key_len = static_cast<uint>(key_str.length());
                        MemCopy(buf, &key_len, sizeof(key_len));
                        buf += sizeof(key_len);
                        MemCopy(buf, key_str.c_str(), key_len);
                        buf += key_len;
                    }
                    else if (prop->IsDictKeyHash()) {
                        const auto hkey = static_cast<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *static_cast<const int*>(key);
                        MemCopy(buf, &ekey, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }
                    else {
                        MemCopy(buf, key, prop->GetDictKeySize());
                        buf += prop->GetDictKeySize();
                    }

                    const auto str_size = static_cast<uint>(str.length());
                    MemCopy(buf, &str_size, sizeof(uint));
                    buf += sizeof(str_size);

                    if (str_size != 0) {
                        MemCopy(buf, str.c_str(), str_size);
                        buf += str_size;
                    }
                }
            }
        }
        else if (prop->IsDictKeyString()) {
            if (dict != nullptr && dict->GetSize() != 0) {
                // Calculate size
                size_t data_size = 0;
                std::vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                const auto value_element_size = prop->GetBaseSize();

                for (auto&& [key, value] : dict_map) {
                    const auto& key_str = *static_cast<const string*>(key);
                    const uint key_len = static_cast<uint>(key_str.length());
                    data_size += sizeof(key_len) + key_len;
                    data_size += value_element_size;
                }

                // Make buffer
                auto* buf = prop_data.Alloc(data_size);

                for (auto&& [key, value] : dict_map) {
                    const auto& key_str = *static_cast<const string*>(key);
                    const uint key_len = static_cast<uint>(key_str.length());

                    MemCopy(buf, &key_len, sizeof(key_len));
                    buf += sizeof(key_len);
                    MemCopy(buf, key_str.c_str(), key_len);
                    buf += key_len;

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = static_cast<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, value_element_size);
                    }
                    else {
                        MemCopy(buf, value, value_element_size);
                    }

                    buf += value_element_size;
                }
            }
        }
        else {
            const auto key_element_size = prop->GetDictKeySize();
            const auto value_element_size = prop->GetBaseSize();
            const auto whole_element_size = key_element_size + value_element_size;
            const auto data_size = dict != nullptr ? dict->GetSize() * whole_element_size : 0;

            if (data_size != 0) {
                auto* buf = prop_data.Alloc(data_size);

                std::vector<pair<void*, void*>> dict_map;
                dict->GetMap(dict_map);

                for (auto&& [key, value] : dict_map) {
                    if (prop->IsDictKeyHash()) {
                        const auto hkey = static_cast<const hstring*>(key)->as_hash();
                        MemCopy(buf, &hkey, key_element_size);
                    }
                    else if (prop->IsDictKeyEnum()) {
                        const auto ekey = *static_cast<const int*>(key);
                        MemCopy(buf, &ekey, key_element_size);
                    }
                    else {
                        MemCopy(buf, key, key_element_size);
                    }

                    buf += key_element_size;

                    if (prop->IsBaseTypeHash()) {
                        const auto hash = static_cast<const hstring*>(value)->as_hash();
                        MemCopy(buf, &hash, value_element_size);
                    }
                    else {
                        MemCopy(buf, value, value_element_size);
                    }

                    buf += value_element_size;
                }
            }
        }
    }
    else {
        UNREACHABLE_PLACE();
    }

    return prop_data;
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, any_t> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>, int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const T& value)
{
    STACK_TRACE_ENTRY();

    if constexpr (std::is_same_v<T, any_t>) {
        out_buf.Write<string>(value);
    }
    else {
        out_buf.Write(value);
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, any_t> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>, int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const vector<T>& value)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(value.size() <= 0xFFFF);
    out_buf.Write(static_cast<uint16>(value.size()));

    for (const auto& inner_value : value) {
        WriteNetBuf(out_buf, inner_value);
    }
}

template<typename T, typename U,
    std::enable_if_t<(std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>) && //
            (std::is_same_v<U, string> || std::is_same_v<U, any_t> || std::is_same_v<U, hstring> || std::is_arithmetic_v<U> || std::is_enum_v<U> || is_strong_type_v<U> || is_valid_pod_type_v<U>),
        int> = 0>
static void WriteNetBuf(NetOutBuffer& out_buf, const map<T, U>& value)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(value.size() <= 0xFFFF);
    out_buf.Write(static_cast<uint16>(value.size()));

    for (const auto& inner_value : value) {
        WriteNetBuf(out_buf, inner_value.first);
        WriteNetBuf(out_buf, inner_value.second);
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, any_t> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>, int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, T& value, HashResolver& hash_resolver)
{
    STACK_TRACE_ENTRY();

    if constexpr (std::is_same_v<T, any_t>) {
        value = any_t {in_buf.Read<string>()};
    }
    else if constexpr (std::is_same_v<T, hstring>) {
        value = in_buf.Read<hstring>(hash_resolver);
    }
    else {
        value = in_buf.Read<T>();
    }
}

template<typename T, std::enable_if_t<std::is_same_v<T, string> || std::is_same_v<T, any_t> || std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>, int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, vector<T>& value, HashResolver& hash_resolver)
{
    STACK_TRACE_ENTRY();

    const auto inner_values_count = in_buf.Read<uint16>();
    value.reserve(inner_values_count);

    for (uint16 i = 0; i < inner_values_count; i++) {
        T inner_value;
        ReadNetBuf(in_buf, inner_value, hash_resolver);
        value.emplace_back(inner_value);
    }
}

template<typename T, typename U,
    std::enable_if_t<(std::is_same_v<T, hstring> || std::is_arithmetic_v<T> || std::is_enum_v<T> || is_strong_type_v<T> || is_valid_pod_type_v<T>) && //
            (std::is_same_v<U, string> || std::is_same_v<U, any_t> || std::is_same_v<U, hstring> || std::is_arithmetic_v<U> || std::is_enum_v<U> || is_strong_type_v<U> || is_valid_pod_type_v<U>),
        int> = 0>
static void ReadNetBuf(NetInBuffer& in_buf, map<T, U>& value, HashResolver& hash_resolver)
{
    STACK_TRACE_ENTRY();

    const auto inner_values_count = in_buf.Read<uint16>();

    for (uint16 i = 0; i < inner_values_count; i++) {
        T inner_value_first;
        ReadNetBuf(in_buf, inner_value_first, hash_resolver);
        U inner_value_second;
        ReadNetBuf(in_buf, inner_value_second, hash_resolver);
        value.emplace(inner_value_first, inner_value_second);
    }
}

[[maybe_unused]] static void WriteRpcHeader(NetOutBuffer& out_buf, uint rpc_num)
{
    STACK_TRACE_ENTRY();

    out_buf.StartMsg(NETMSG_REMOTE_CALL);
    out_buf.Write(rpc_num);
}

[[maybe_unused]] static void WriteRpcFooter(NetOutBuffer& out_buf)
{
    STACK_TRACE_ENTRY();

    out_buf.EndMsg();
}
#endif

template<typename T>
static void Entity_AddRef(const T* self)
{
    NO_STACK_TRACE_ENTRY();

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
    NO_STACK_TRACE_ENTRY();

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
    NO_STACK_TRACE_ENTRY();

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
    NO_STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

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
static auto Entity_Id(const T* self) -> ident_t
{
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

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
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(self);
    ENTITY_VERIFY(self);

    if (const auto* self_proto = dynamic_cast<const ProtoEntity*>(self)) {
        return self_proto->GetProtoId();
    }
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
    STACK_TRACE_ENTRY();

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

template<int Args>
static void Global_Assert(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto condition = *static_cast<bool*>(gen->GetAddressOfArg(0));

    if (condition) {
        return;
    }

    vector<string> obj_infos;
    obj_infos.reserve(Args);

    for (int i = 1; i < gen->GetArgCount(); i++) {
        const auto* obj = *static_cast<void**>(gen->GetAddressOfArg(i));
        const auto obj_type_id = gen->GetArgTypeId(i);
        obj_infos.emplace_back(GetASObjectInfo(obj, obj_type_id));
    }

    if constexpr (Args == 0) {
        throw ScriptException("Assertion failed");
    }
    else if constexpr (Args == 1) {
        throw ScriptException("Assertion failed", obj_infos[0]);
    }
    else if constexpr (Args == 2) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1]);
    }
    else if constexpr (Args == 3) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2]);
    }
    else if constexpr (Args == 4) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3]);
    }
    else if constexpr (Args == 5) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4]);
    }
    else if constexpr (Args == 6) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5]);
    }
    else if constexpr (Args == 7) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6]);
    }
    else if constexpr (Args == 8) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7]);
    }
    else if constexpr (Args == 9) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7], obj_infos[8]);
    }
    else if constexpr (Args == 10) {
        throw ScriptException("Assertion failed", obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7], obj_infos[8], obj_infos[9]);
    }
    else {
        UNREACHABLE_PLACE();
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<int Args>
static void Global_ThrowException(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto& message = *static_cast<string*>(gen->GetAddressOfArg(0));

    vector<string> obj_infos;
    obj_infos.reserve(Args);

    for (int i = 1; i < gen->GetArgCount(); i++) {
        const auto* obj = *static_cast<void**>(gen->GetAddressOfArg(i));
        const auto obj_type_id = gen->GetArgTypeId(i);
        obj_infos.emplace_back(GetASObjectInfo(obj, obj_type_id));
    }

    if constexpr (Args == 0) {
        throw ScriptException(message);
    }
    else if constexpr (Args == 1) {
        throw ScriptException(message, obj_infos[0]);
    }
    else if constexpr (Args == 2) {
        throw ScriptException(message, obj_infos[0], obj_infos[1]);
    }
    else if constexpr (Args == 3) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2]);
    }
    else if constexpr (Args == 4) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3]);
    }
    else if constexpr (Args == 5) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4]);
    }
    else if constexpr (Args == 6) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5]);
    }
    else if constexpr (Args == 7) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6]);
    }
    else if constexpr (Args == 8) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7]);
    }
    else if constexpr (Args == 9) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7], obj_infos[8]);
    }
    else if constexpr (Args == 10) {
        throw ScriptException(message, obj_infos[0], obj_infos[1], obj_infos[2], obj_infos[3], obj_infos[4], obj_infos[5], obj_infos[6], obj_infos[7], obj_infos[8], obj_infos[9]);
    }
    else {
        UNREACHABLE_PLACE();
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void Global_Yield(uint time)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    auto* ctx = asGetActiveContext();
    RUNTIME_ASSERT(ctx);
    auto* game_engine = static_cast<FOEngine*>(ctx->GetEngine()->GetUserData());
    auto& ctx_ext = GET_CONTEXT_EXT(ctx);
    ctx_ext.SuspendEndTime = game_engine->GameTime.FrameTime() + std::chrono::milliseconds {time};
    ctx->Suspend();

#else
    UNUSED_VARIABLE(time);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void ASPropertyGetter(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto* registrator = engine->GetPropertyRegistrator(T::ENTITY_TYPE_NAME);
    const auto prop_index = *static_cast<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));
    const auto* prop = registrator->GetPropertyByIndex(static_cast<int>(prop_index));
    auto* as_engine = gen->GetEngine();
    auto* script_sys = GET_SCRIPT_SYS_FROM_ENTITY(engine);

    if (auto* as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); as_type_info == nullptr || as_type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto* func = *reinterpret_cast<asIScriptFunction**>(gen->GetArgAddress(1));

    if (!prop->IsVirtual()) {
        throw ScriptException("Property is not virtual", prop->GetName());
    }
    if (func->GetParamCount() != 1 && func->GetParamCount() != 2) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() == asTYPEID_VOID) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }
    if (prop->GetFullTypeName() != strex(as_engine->GetTypeDeclaration(func->GetReturnTypeId())).replace("[]@", "[]").str()) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    int type_id;
    asDWORD flags;
    int as_result;
    AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (auto* as_type_info = as_engine->GetTypeInfoById(type_id); as_type_info == nullptr || string_view(as_type_info->GetName()) != T::ENTITY_TYPE_NAME || flags != 0) {
        throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
    }

    if (func->GetParamCount() == 2) {
        AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (type_id != gen->GetArgTypeId(0) || flags != 0) {
            throw ScriptException("Invalid getter function", prop->GetName(), func->GetName());
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
            ctx->SetArgObject(0, actual_entity); // May be null for protos
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
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T, typename Deferred>
static void ASPropertySetter(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    const auto* registrator = engine->GetPropertyRegistrator(T::ENTITY_TYPE_NAME);
    const auto prop_index = *static_cast<ScriptEnum_uint16*>(gen->GetAddressOfArg(0));
    const auto* prop = registrator->GetPropertyByIndex(static_cast<int>(prop_index));
    auto* as_engine = gen->GetEngine();
    auto* script_sys = GET_SCRIPT_SYS_FROM_ENTITY(engine);

    if (auto* as_type_info = as_engine->GetTypeInfoById(gen->GetArgTypeId(1)); as_type_info == nullptr || as_type_info->GetFuncdefSignature() == nullptr) {
        throw ScriptException("Invalid function object", prop->GetName());
    }

    auto* func = *reinterpret_cast<asIScriptFunction**>(gen->GetArgAddress(1));

    if (func->GetParamCount() != 1 && func->GetParamCount() != 2 && func->GetParamCount() != 3) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }
    if (func->GetReturnTypeId() != asTYPEID_VOID) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    int type_id;
    asDWORD flags;
    int as_result;
    AS_VERIFY(func->GetParam(0, &type_id, &flags));

    if (auto* as_type_info = as_engine->GetTypeInfoById(type_id); as_type_info == nullptr || string_view(as_type_info->GetName()) != T::ENTITY_TYPE_NAME || flags != 0) {
        throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
    }

    bool has_proto_enum = false;
    bool has_value_ref = false;

    if (func->GetParamCount() > 1) {
        AS_VERIFY(func->GetParam(1, &type_id, &flags));

        if (prop->GetFullTypeName() == strex(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == asTM_INOUTREF) {
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
            AS_VERIFY(func->GetParam(2, &type_id, &flags));

            if (prop->GetFullTypeName() == strex(as_engine->GetTypeDeclaration(type_id)).replace("[]@", "[]").str() && flags == asTM_INOUTREF) {
                has_value_ref = true;
            }
            else {
                throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
            }
        }
    }

    if constexpr (Deferred::value) {
        if (has_value_ref) {
            throw ScriptException("Invalid setter function", prop->GetName(), func->GetName());
        }
    }

    prop->AddSetter([prop, func, script_sys, has_proto_enum, has_value_ref](Entity* entity, const Property*, PropertyRawData& prop_data) -> void {
        ENTITY_VERIFY_NULL(entity);
        ENTITY_VERIFY(entity);

        auto* ctx = script_sys->PrepareContext(func);
        auto& ctx_ext = GET_CONTEXT_EXT(ctx);

        ctx_ext.ValidCheck = entity;

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
            UNUSED_VARIABLE(prop_data);
        }
    });

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Global_Get(asIScriptGeneric* gen)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto& value = *static_cast<T*>(gen->GetAuxiliary());
    *static_cast<T*>(gen->GetAddressOfReturnLocation()) = value;

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Entity_GetSelfForEvent(T* entity) -> T*
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    // Don't verify entity for destroying
    ENTITY_VERIFY_NULL(entity);
    return entity;

#else
    UNUSED_VARIABLE(entity);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Property_GetValueAsInt(const T* entity, int prop_index) -> int
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);
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
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);
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
static auto Property_GetValueAsAny(const T* entity, int prop_index) -> any_t
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);
    RUNTIME_ASSERT(prop);

    if (!prop->IsPlainData()) {
        throw ScriptException("Property in not plain data type");
    }
    if (prop->IsDisabled()) {
        throw ScriptException("Property is disabled");
    }

    return entity->GetValueAsAny(prop);

#else
    UNUSED_VARIABLE(entity);
    UNUSED_VARIABLE(prop_index);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_SetValueAsAny(T* entity, int prop_index, any_t value)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    ENTITY_VERIFY_NULL(entity);
    ENTITY_VERIFY(entity);

    if (prop_index == -1) {
        throw ScriptException("Property invalid enum");
    }

    const auto* prop = entity->GetProperties().GetRegistrator()->GetPropertyByIndex(prop_index);
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

    entity->SetValueAsAny(prop, value);

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
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    auto* entity = static_cast<T*>(gen->GetObject());
    const auto& component = *static_cast<const hstring*>(gen->GetAuxiliary());

    if (auto* proto_entity = dynamic_cast<const ProtoEntity*>(entity); proto_entity != nullptr) {
        if (proto_entity->HasComponent(component)) {
            *(T**)gen->GetAddressOfReturnLocation() = entity;
            return;
        }
    }
    if (auto* entity_with_proto = dynamic_cast<const EntityWithProto*>(entity); entity_with_proto != nullptr) {
        if (entity_with_proto->GetProto()->HasComponent(component)) {
            *(T**)gen->GetAddressOfReturnLocation() = entity;
            return;
        }
    }

    *(T**)gen->GetAddressOfReturnLocation() = nullptr;

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_GetValue(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

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
        const auto& props = entity->GetProperties();

        props.ValidateForRawData(prop);

        const auto prop_raw_data = props.GetRawData(prop);

        prop_data.Pass(prop_raw_data);
    }

    PropsToAS(prop, prop_data, gen->GetAddressOfReturnLocation(), gen->GetEngine());

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Property_SetValue(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

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
        props.SetRawData(prop, {prop_data.GetPtrAs<uint8>(), prop_data.GetSize()});

        if (!post_setters.empty()) {
            for (auto& setter : post_setters) {
                setter(entity, prop);
            }
        }
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto EntityDownCast(T* a) -> Entity*
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return static_cast<Entity*>(a);

#else
    UNUSED_VARIABLE(a);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto EntityUpCast(Entity* a) -> T*
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    static_assert(std::is_base_of_v<Entity, T>);

    if (a == nullptr) {
        return nullptr;
    }

    return dynamic_cast<T*>(a);

#else
    UNUSED_VARIABLE(a);
    throw ScriptCompilerException("Stub");
#endif
}

static void HashedString_Construct(hstring* self)
{
    NO_STACK_TRACE_ENTRY();

    new (self) hstring();
}

static void HashedString_ConstructFromString(asIScriptGeneric* gen)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto* str = *static_cast<const string**>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    auto hstr = engine->ToHashedString(*str);
    new (gen->GetObject()) hstring(hstr);

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void HashedString_IsValidHash(asIScriptGeneric* gen)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto& hash = *static_cast<const uint*>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    bool failed = false;
    auto hstr = engine->ResolveHash(hash, &failed);
    UNUSED_VARIABLE(hstr);
    new (gen->GetAddressOfReturnLocation()) bool(!failed);

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void HashedString_CreateFromHash(asIScriptGeneric* gen)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto& hash = *static_cast<const uint*>(gen->GetAddressOfArg(0));
    auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
    auto hstr = engine->ResolveHash(hash);
    new (gen->GetAddressOfReturnLocation()) hstring(hstr);

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void HashedString_ConstructCopy(hstring* self, const hstring& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) hstring(other);

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static void HashedString_Assign(hstring& self, const hstring& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    self = other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_Equals(const hstring& self, const hstring& other) -> bool
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self == other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_EqualsString(const hstring& self, const string& other) -> bool
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.as_str() == other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_StringCast(const hstring& self) -> const string&
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.as_str();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_StringConv(const hstring& self) -> string
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.as_str();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_GetString(const hstring& self) -> string
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return string(self.as_str());

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_GetHash(const hstring& self) -> int
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.as_int();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static auto HashedString_GetUHash(const hstring& self) -> uint
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.as_uint();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static void Any_Construct(any_t* self)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) any_t();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static void Any_Destruct(any_t* self)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    self->~any_t();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Any_ConstructFrom(any_t* self, const T& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) any_t {strex("{}", other).str()};

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static void Any_ConstructCopy(any_t* self, const any_t& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) any_t(other);

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static void Any_Assign(any_t& self, const any_t& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    self = other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

static auto Any_Equals(const any_t& self, const any_t& other) -> bool
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self == other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Any_Conv(const any_t& self) -> T
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    if constexpr (std::is_same_v<T, bool>) {
        return strex(self).toBool();
    }
    else if constexpr (is_strong_type_v<T>) {
        return T {static_cast<typename T::underlying_type>(strex(self).toInt64())};
    }
    else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(strex(self).toInt64());
    }
    else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(strex(self).toDouble());
    }
    else if constexpr (std::is_same_v<T, string>) {
        return self;
    }
    else {
        return parse_from_string<T>(self);
    }

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Any_ConvGen(asIScriptGeneric* gen)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    if constexpr (std::is_same_v<T, hstring>) {
        auto* self = static_cast<any_t*>(gen->GetObject());
        auto* engine = static_cast<FOEngine*>(gen->GetAuxiliary());
        auto hstr = engine->ToHashedString(*self);
        new (gen->GetAddressOfReturnLocation()) hstring(hstr);
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Any_ConvFrom(const T& self) -> any_t
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return any_t {strex("{}", self).str()};

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void StrongType_Construct(T* self)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) T();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void StrongType_ConstructCopy(T* self, const T& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) T(other);

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void StrongType_ConstructFromUnderlying(T* self, const typename T::underlying_type& other)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) T(other);

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto StrongType_Equals(const T& self, const T& other) -> bool
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self == other;

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(other);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto StrongType_UnderlyingConv(const T& self) -> typename T::underlying_type
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return self.underlying_value();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto StrongType_GetStr(const T& self) -> string
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return strex("{}", self).str();

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto StrongType_AnyConv(const T& self) -> any_t
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    return any_t {strex("{}", self).str()};

#else
    UNUSED_VARIABLE(self);
    throw ScriptCompilerException("Stub");
#endif
}

static void Ucolor_ConstructRawRgba(ucolor* self, uint rgba)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    new (self) ucolor {rgba};

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(rgba);
    throw ScriptCompilerException("Stub");
#endif
}

static void Ucolor_ConstructRgba(ucolor* self, int r, int g, int b, int a)
{
    NO_STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto clamped_r = static_cast<uint8>(std::clamp(r, 0, 255));
    const auto clamped_g = static_cast<uint8>(std::clamp(g, 0, 255));
    const auto clamped_b = static_cast<uint8>(std::clamp(b, 0, 255));
    const auto clamped_a = static_cast<uint8>(std::clamp(a, 0, 255));

    new (self) ucolor {clamped_r, clamped_g, clamped_b, clamped_a};

#else
    UNUSED_VARIABLE(self);
    UNUSED_VARIABLE(r, g, b, a);
    throw ScriptCompilerException("Stub");
#endif
}

static void Ipos_ConstructXandY(ipos* self, int x, int y)
{
    NO_STACK_TRACE_ENTRY();

    new (self) ipos {x, y};
}

static auto Ipos_AddAssignIpos(ipos& self, const ipos& pos) -> ipos&
{
    NO_STACK_TRACE_ENTRY();

    self.x += pos.x;
    self.y += pos.y;
    return self;
}

static auto Ipos_SubAssignIpos(ipos& self, const ipos& pos) -> ipos&
{
    NO_STACK_TRACE_ENTRY();

    self.x -= pos.x;
    self.y -= pos.y;
    return self;
}

static auto Ipos_AddIpos(const ipos& self, const ipos& pos) -> ipos
{
    NO_STACK_TRACE_ENTRY();

    return ipos {self.x + pos.x, self.y + pos.y};
}

static auto Ipos_SubIpos(const ipos& self, const ipos& pos) -> ipos
{
    NO_STACK_TRACE_ENTRY();

    return ipos {self.x - pos.x, self.y - pos.y};
}

static auto Ipos_NegIpos(const ipos& self) -> ipos
{
    NO_STACK_TRACE_ENTRY();

    return ipos {-self.x, -self.y};
}

static auto Ipos_AddAssignIsize(ipos& self, const isize& size) -> ipos&
{
    NO_STACK_TRACE_ENTRY();

    self.x += size.width;
    self.y += size.height;
    return self;
}

static auto Ipos_SubAssignIsize(ipos& self, const isize& size) -> ipos&
{
    NO_STACK_TRACE_ENTRY();

    self.x -= size.width;
    self.y -= size.height;
    return self;
}

static auto Ipos_AddIsize(const ipos& self, const isize& size) -> ipos
{
    NO_STACK_TRACE_ENTRY();

    return ipos {self.x + size.width, self.y + size.height};
}

static auto Ipos_SubIsize(const ipos& self, const isize& size) -> ipos
{
    NO_STACK_TRACE_ENTRY();

    return ipos {self.x - size.width, self.y - size.height};
}

static auto Ipos_FitToSize(const ipos& self, isize size) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return self.x >= 0 && self.y >= 0 && self.x < size.width && self.y < size.height;
}

static auto Ipos_FitToRect(const ipos& self, irect rect) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return self.x >= rect.x && self.y >= rect.x && self.x < rect.x + rect.width && self.y < rect.y + rect.height;
}

static void Isize_ConstructWandH(isize* self, int width, int height)
{
    NO_STACK_TRACE_ENTRY();

    new (self) isize {width, height};
}

static void Irect_ConstructXandYandWandH(irect* self, int x, int y, int width, int height)
{
    NO_STACK_TRACE_ENTRY();

    new (self) irect {x, y, width, height};
}

static void Fpos_ConstructXandY(fpos* self, float x, float y)
{
    NO_STACK_TRACE_ENTRY();

    new (self) fpos {x, y};
}

static void Mpos_ConstructXandY(mpos* self, int x, int y)
{
    NO_STACK_TRACE_ENTRY();

    if (x < 0 || x > 0xFFFF || y < 0 || y > 0xFFFF) {
        throw ScriptException("Invalid mpos values", x, y);
    }

    new (self) mpos {static_cast<uint16>(x), static_cast<uint16>(y)};
}

static auto Mpos_FitToSize(const mpos& self, msize size) -> bool
{
    NO_STACK_TRACE_ENTRY();

    return self.x >= 0 && self.y >= 0 && self.x < size.width && self.y < size.height;
}

template<typename T>
static auto Game_GetProtoCustomEntity(FOEngine* engine, hstring pid) -> const ProtoCustomEntity*
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto entity_type = engine->ToHashedString(T::ENTITY_TYPE_NAME);
    const auto* proto = engine->ProtoMngr.GetProtoEntitySafe(entity_type, pid);
    if (proto != nullptr) {
        const auto* casted_proto = dynamic_cast<const ProtoCustomEntity*>(proto);
        RUNTIME_ASSERT(casted_proto);
        return casted_proto;
    }
    return nullptr;

#else
    UNUSED_VARIABLE(engine, pid);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static auto Game_GetProtoCustomEntities(FOEngine* engine) -> CScriptArray*
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    const auto entity_type = engine->ToHashedString(T::ENTITY_TYPE_NAME);
    const auto& protos = engine->ProtoMngr.GetProtoEntities(entity_type);

    vector<const ProtoCustomEntity*> result;
    result.reserve(protos.size());

    for (auto&& [pid, proto] : protos) {
        const auto* casted_proto = dynamic_cast<const ProtoCustomEntity*>(proto.get());
        RUNTIME_ASSERT(casted_proto);
        result.emplace_back(casted_proto);
    }

    auto* as_engine = GET_AS_ENGINE_FROM_ENTITY(engine);
    CScriptArray* result_arr = MarshalBackArray(as_engine, strex("{}[]", T::ENTITY_TYPE_NAME).c_str(), result);
    return result_arr;

#else
    UNUSED_VARIABLE(engine);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Game_DestroyOne(FOEngine* engine, T* entity)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
#if SERVER_SCRIPTING
    if (entity != nullptr) {
        engine->EntityMngr.DestroyEntity(entity);
    }
#else
    UNUSED_VARIABLE(engine, entity);
#endif

#else
    UNUSED_VARIABLE(engine);
    UNUSED_VARIABLE(entity);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename T>
static void Game_DestroyAll(FOEngine* engine, CScriptArray* as_entities)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
#if SERVER_SCRIPTING
    auto* as_engine = GET_AS_ENGINE_FROM_ENTITY(engine);
    const auto entities = MarshalArray<T*>(as_engine, as_entities);
    for (auto* entity : entities) {
        if (entity != nullptr) {
            engine->EntityMngr.DestroyEntity(entity);
        }
    }
#else
    UNUSED_VARIABLE(engine, as_entities);
#endif

#else
    UNUSED_VARIABLE(engine);
    UNUSED_VARIABLE(as_entities);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename H, typename T, typename T2>
static void CustomEntity_Add(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
#if SERVER_SCRIPTING
    hstring entry = *static_cast<hstring*>(gen->GetAuxiliary());
    H* holder = static_cast<H*>(gen->GetObject());
    const hstring pid = gen->GetArgCount() == 1 ? *static_cast<hstring*>(gen->GetAddressOfArg(0)) : hstring();
    CustomEntity* entity = holder->GetEngine()->EntityMngr.CreateCustomInnerEntity(holder, entry, pid);
    RUNTIME_ASSERT(entity->GetTypeName() == T2::ENTITY_TYPE_NAME);
    new (gen->GetAddressOfReturnLocation()) CustomEntity*(entity);
#else
    UNUSED_VARIABLE(gen);
#endif

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename H, typename T, typename T2>
static void CustomEntity_GetAll(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    hstring entry = *static_cast<hstring*>(gen->GetAuxiliary());
    H* holder = static_cast<H*>(gen->GetObject());
    auto* entities = holder->GetInnerEntities(entry);

    if (entities != nullptr && !entities->empty()) {
        vector<T*> casted_entities;
        casted_entities.reserve(entities->size());

        for (auto* entity : *entities) {
            ENTITY_VERIFY(entity);
            RUNTIME_ASSERT(entity->GetTypeName() == T2::ENTITY_TYPE_NAME);
            auto* casted_entity = dynamic_cast<T*>(entity);
            RUNTIME_ASSERT(casted_entity);
            casted_entities.emplace_back(casted_entity);
        }

        CScriptArray* result_arr = MarshalBackArray(gen->GetEngine(), strex("{}[]", T2::ENTITY_TYPE_NAME).c_str(), casted_entities);
        new (gen->GetAddressOfReturnLocation()) CScriptArray*(result_arr);
    }
    else {
        CScriptArray* result_arr = CreateASArray(gen->GetEngine(), strex("{}[]", T2::ENTITY_TYPE_NAME).c_str());
        new (gen->GetAddressOfReturnLocation()) CScriptArray*(result_arr);
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

template<typename H, typename T, typename T2>
static void CustomEntity_GetAllIds(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

#if !COMPILER_MODE
    hstring entry = *static_cast<hstring*>(gen->GetAuxiliary());
    H* holder = static_cast<H*>(gen->GetObject());
    auto* entities = holder->GetInnerEntities(entry);

    if (entities != nullptr && !entities->empty()) {
        vector<ident_t> ids;
        ids.reserve(entities->size());

        for (auto* entity : *entities) {
            ENTITY_VERIFY(entity);
            RUNTIME_ASSERT(entity->GetTypeName() == T2::ENTITY_TYPE_NAME);
            auto* casted_entity = dynamic_cast<T*>(entity);
            RUNTIME_ASSERT(casted_entity);
            ids.emplace_back(casted_entity->GetId());
        }

        CScriptArray* result_arr = MarshalBackArray(gen->GetEngine(), "ident[]", ids);
        new (gen->GetAddressOfReturnLocation()) CScriptArray*(result_arr);
    }
    else {
        CScriptArray* result_arr = CreateASArray(gen->GetEngine(), "ident[]");
        new (gen->GetAddressOfReturnLocation()) CScriptArray*(result_arr);
    }

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void Enum_Parse(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

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
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void Enum_ToString(asIScriptGeneric* gen)
{
    STACK_TRACE_ENTRY();

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
        enum_value_name = strex("{}::{}", enum_info->EnumName, enum_value_name);
    }

    new (gen->GetAddressOfReturnLocation()) string(std::move(enum_value_name));

#else
    UNUSED_VARIABLE(gen);
    throw ScriptCompilerException("Stub");
#endif
}

static void CallbackMessage(const asSMessageInfo* msg, void* param)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(param);

    const char* type = "error";
    if (msg->type == asMSGTYPE_WARNING) {
        type = "warning";
    }
    else if (msg->type == asMSGTYPE_INFORMATION) {
        type = "info";
    }

    const auto formatted_message = strex("{}({},{}): {} : {}", Preprocessor::ResolveOriginalFile(msg->row), Preprocessor::ResolveOriginalLine(msg->row), msg->col, type, msg->message).str();

#if COMPILER_MODE
    extern unordered_set<string> CompilerPassedMessages;
    if (CompilerPassedMessages.count(formatted_message) == 0) {
        CompilerPassedMessages.insert(formatted_message);
    }
    else {
        return;
    }
#endif

    WriteLog("{}", formatted_message);
}

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
static void CompileRootModule(asIScriptEngine* engine, const FileSystem& resources);
#else
static void RestoreRootModule(asIScriptEngine* engine, const_span<uint8> script_bin);
#endif

void SCRIPTING_CLASS::InitAngelScriptScripting(INIT_ARGS)
{
    STACK_TRACE_ENTRY();

#if COMPILER_VALIDATION_MODE
    UNUSED_VARIABLE(resources);
#endif

#if !COMPILER_MODE
    FOEngine* game_engine = _engine;
    game_engine->AddRef();
#else
    FOEngine* game_engine = SafeAlloc::MakeRaw<FOEngine>();
    game_engine->ScriptSys.reset(this);
#endif

    auto game_engine_releaser = ScopeCallback([game_engine]() noexcept {
#if COMPILER_MODE
        (void)game_engine->ScriptSys.release();
#endif
        game_engine->Release();
    });

#if COMPILER_MODE
    static int dummy = 0;
#endif

#if COMPILER_MODE
#if SERVER_SCRIPTING
    MapEngineEntityType<Player>("Player");
    MapEngineEntityType<Item>("Item");
    MapEngineEntityType<Critter>("Critter");
    MapEngineEntityType<Map>("Map");
    MapEngineEntityType<Location>("Location");
#else
    MapEngineEntityType<PlayerView>("Player");
    MapEngineEntityType<ItemView>("Item");
    MapEngineEntityType<CritterView>("Critter");
    MapEngineEntityType<MapView>("Map");
    MapEngineEntityType<LocationView>("Location");
#endif
#endif

    asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    RUNTIME_ASSERT(engine);

    engine->SetUserData(game_engine);

#if !COMPILER_MODE
    AngelScriptData = SafeAlloc::MakeUnique<AngelScriptImpl>();
    AngelScriptData->GameEngine = game_engine;
    AngelScriptData->Engine = engine;
#endif

#if !COMPILER_MODE
    [[maybe_unused]] unordered_set<hstring>& hashed_strings = AngelScriptData->HashedStrings;
#else
    [[maybe_unused]] unordered_set<hstring> hashed_strings;
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
        AS_VERIFY(engine->RegisterGlobalFunction(strex("{} Parse_{}(string valueName)", enum_name, enum_name).c_str(), SCRIPT_GENERIC(Enum_Parse), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->RegisterGlobalFunction(strex("{} Parse(string valueName, {} defaultValue)", enum_name, enum_name).c_str(), SCRIPT_GENERIC(Enum_Parse), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->RegisterGlobalFunction(strex("string ToString({} value, bool fullSpecification = false)", enum_name).c_str(), SCRIPT_GENERIC(Enum_ToString), SCRIPT_GENERIC_CONV, enum_info));
        AS_VERIFY(engine->SetDefaultNamespace(""));
    }

    // Register hstring
    AS_VERIFY(engine->RegisterObjectType("hstring", sizeof(hstring), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<hstring>()));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS(HashedString_Construct), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_ConstructCopy), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("hstring", asBEHAVE_CONSTRUCT, "void f(const string &in)", SCRIPT_GENERIC(HashedString_ConstructFromString), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("bool hstring_isValidHash(int h)", SCRIPT_GENERIC(HashedString_IsValidHash), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterGlobalFunction("hstring hstring_fromHash(int h)", SCRIPT_GENERIC(HashedString_CreateFromHash), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "hstring& opAssign(const hstring &in)", SCRIPT_FUNC_THIS(HashedString_Assign), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const hstring &in) const", SCRIPT_FUNC_THIS(HashedString_Equals), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "bool opEquals(const string &in) const", SCRIPT_FUNC_THIS(HashedString_EqualsString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "const string& opImplCast() const", SCRIPT_FUNC_THIS(HashedString_StringCast), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string opImplConv() const", SCRIPT_FUNC_THIS(HashedString_StringConv), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "string get_str() const", SCRIPT_FUNC_THIS(HashedString_GetString), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "int get_hash() const", SCRIPT_FUNC_THIS(HashedString_GetHash), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "uint get_uhash() const", SCRIPT_FUNC_THIS(HashedString_GetUHash), SCRIPT_FUNC_THIS_CONV));
    static hstring EMPTY_hstring;
    AS_VERIFY(engine->RegisterGlobalFunction("hstring get_EMPTY_HSTRING()", SCRIPT_GENERIC((Global_Get<hstring>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(EMPTY_hstring)));

    // Register any
    AS_VERIFY(engine->RegisterObjectType("any", sizeof(any_t), asOBJ_VALUE | asGetTypeTraits<string>()));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS(Any_Construct), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const any &in)", SCRIPT_FUNC_THIS(Any_ConstructCopy), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const bool &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<bool>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const int8 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<int8>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const uint8 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<uint8>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const int16 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<int16>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const uint16 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<uint16>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const int &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<int>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const uint &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<uint>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const int64 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<int64>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const uint64 &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<uint64>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const float &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<float>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_CONSTRUCT, "void f(const double &in)", SCRIPT_FUNC_THIS(Any_ConstructFrom<double>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("any", asBEHAVE_DESTRUCT, "void f()", SCRIPT_FUNC_THIS(Any_Destruct), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "any &opAssign(const any &in)", SCRIPT_FUNC_THIS(Any_Assign), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "bool opEquals(const any &in) const", SCRIPT_FUNC_THIS(Any_Equals), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "bool opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<bool>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "int8 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<int8>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "uint8 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<uint8>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "int16 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<int16>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "uint16 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<uint16>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "int opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<int>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "uint opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<uint>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "int64 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<int64>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "uint64 opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<uint64>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "float opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<float>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "double opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<double>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "string opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<string>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("any", "hstring opImplConv() const", SCRIPT_GENERIC(Any_ConvGen<hstring>), SCRIPT_GENERIC_CONV, game_engine));
    AS_VERIFY(engine->RegisterObjectMethod("string", "any opImplConv() const", SCRIPT_FUNC_THIS(Any_ConvFrom<string>), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("hstring", "any opImplConv() const", SCRIPT_FUNC_THIS(Any_ConvFrom<hstring>), SCRIPT_FUNC_THIS_CONV));
    ScriptExtensions::RegisterScriptStdStringAnyExtensions(engine);

    // Global functions
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition)", SCRIPT_GENERIC(Global_Assert<0>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1)", SCRIPT_GENERIC(Global_Assert<1>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2)", SCRIPT_GENERIC(Global_Assert<2>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3)", SCRIPT_GENERIC(Global_Assert<3>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", SCRIPT_GENERIC(Global_Assert<4>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", SCRIPT_GENERIC(Global_Assert<5>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", SCRIPT_GENERIC(Global_Assert<6>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", SCRIPT_GENERIC(Global_Assert<7>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", SCRIPT_GENERIC(Global_Assert<8>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", SCRIPT_GENERIC(Global_Assert<9>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Assert(bool condition, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", SCRIPT_GENERIC(Global_Assert<10>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message)", SCRIPT_GENERIC(Global_ThrowException<0>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1)", SCRIPT_GENERIC(Global_ThrowException<1>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2)", SCRIPT_GENERIC(Global_ThrowException<2>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3)", SCRIPT_GENERIC(Global_ThrowException<3>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4)", SCRIPT_GENERIC(Global_ThrowException<4>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5)", SCRIPT_GENERIC(Global_ThrowException<5>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6)", SCRIPT_GENERIC(Global_ThrowException<6>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7)", SCRIPT_GENERIC(Global_ThrowException<7>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8)", SCRIPT_GENERIC(Global_ThrowException<8>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9)", SCRIPT_GENERIC(Global_ThrowException<9>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void ThrowException(string message, ?&in obj1, ?&in obj2, ?&in obj3, ?&in obj4, ?&in obj5, ?&in obj6, ?&in obj7, ?&in obj8, ?&in obj9, ?&in obj10)", SCRIPT_GENERIC(Global_ThrowException<10>), SCRIPT_GENERIC_CONV));
    AS_VERIFY(engine->RegisterGlobalFunction("void Yield(uint duration)", SCRIPT_FUNC(Global_Yield), SCRIPT_FUNC_CONV));

    // Strong type registrators
#define REGISTER_HARD_STRONG_TYPE(name, type) \
    if (strong_type_registered.count(name) == 0) { \
        strong_type_registered.emplace(name); \
        AS_VERIFY(engine->RegisterObjectType(name, sizeof(type), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<type>())); \
        AS_VERIFY(engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, "void f()", SCRIPT_FUNC_THIS((StrongType_Construct<type>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, "void f(const " name " &in)", SCRIPT_FUNC_THIS((StrongType_ConstructCopy<type>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod(name, "bool opEquals(const " name " &in) const", SCRIPT_FUNC_THIS((StrongType_Equals<type>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod(name, "string get_str() const", SCRIPT_FUNC_THIS(StrongType_GetStr<type>), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod(name, "any opImplConv() const", SCRIPT_FUNC_THIS(StrongType_AnyConv<type>), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("any", name " opImplConv() const", SCRIPT_FUNC_THIS(Any_Conv<type>), SCRIPT_FUNC_THIS_CONV)); \
        static type ZERO_##type; \
        AS_VERIFY(engine->RegisterGlobalFunction(strex(name " get_ZERO_{}()", strex(name).upper()).c_str(), SCRIPT_GENERIC((Global_Get<type>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(ZERO_##type))); \
    }

#define REGISTER_RELAXED_STRONG_TYPE(name, type) \
    static_assert(is_strong_type_v<type>); \
    if (strong_type_registered.count(name) == 0) { \
        REGISTER_HARD_STRONG_TYPE(name, type); \
        AS_VERIFY(engine->RegisterObjectBehaviour(name, asBEHAVE_CONSTRUCT, strex("void f(const {} &in)", type::underlying_type_name).c_str(), SCRIPT_FUNC_THIS((StrongType_ConstructFromUnderlying<type>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod(name, strex("{} opImplConv() const", type::underlying_type_name).c_str(), SCRIPT_FUNC_THIS((StrongType_UnderlyingConv<type>)), SCRIPT_FUNC_THIS_CONV)); \
    }

#define REGISTER_STRONG_TYPE_VALUE_ACCESSOR(name, type) AS_VERIFY(engine->RegisterObjectProperty(name, strex("{} value", type::underlying_type_name).c_str(), 0));

    unordered_set<string> strong_type_registered;

    // Register ucolor
    REGISTER_HARD_STRONG_TYPE("ucolor", ucolor);
    AS_VERIFY(engine->RegisterObjectBehaviour("ucolor", asBEHAVE_CONSTRUCT, "void f(uint rgba)", SCRIPT_FUNC_THIS(Ucolor_ConstructRawRgba), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectBehaviour("ucolor", asBEHAVE_CONSTRUCT, "void f(int r, int g, int b, int a = 255)", SCRIPT_FUNC_THIS(Ucolor_ConstructRgba), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("ucolor", "uint8 red", offsetof(ucolor, comp.r)));
    AS_VERIFY(engine->RegisterObjectProperty("ucolor", "uint8 green", offsetof(ucolor, comp.g)));
    AS_VERIFY(engine->RegisterObjectProperty("ucolor", "uint8 blue", offsetof(ucolor, comp.b)));
    AS_VERIFY(engine->RegisterObjectProperty("ucolor", "uint8 alpha", offsetof(ucolor, comp.a)));

    // Register ipos
    REGISTER_HARD_STRONG_TYPE("ipos", ipos);
    AS_VERIFY(engine->RegisterObjectBehaviour("ipos", asBEHAVE_CONSTRUCT, "void f(int x, int y)", SCRIPT_FUNC_THIS(Ipos_ConstructXandY), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("ipos", "int x", offsetof(ipos, x)));
    AS_VERIFY(engine->RegisterObjectProperty("ipos", "int y", offsetof(ipos, y)));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos& opAddAssign(const ipos &in)", SCRIPT_FUNC_THIS(Ipos_AddAssignIpos), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos& opSubAssign(const ipos &in)", SCRIPT_FUNC_THIS(Ipos_SubAssignIpos), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos opAdd(const ipos &in) const", SCRIPT_FUNC_THIS(Ipos_AddIpos), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos opSub(const ipos &in) const", SCRIPT_FUNC_THIS(Ipos_SubIpos), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos opNeg() const", SCRIPT_FUNC_THIS(Ipos_NegIpos), SCRIPT_FUNC_THIS_CONV));

    // Register isize
    REGISTER_HARD_STRONG_TYPE("isize", isize);
    AS_VERIFY(engine->RegisterObjectBehaviour("isize", asBEHAVE_CONSTRUCT, "void f(int width, int height)", SCRIPT_FUNC_THIS(Isize_ConstructWandH), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("isize", "int width", offsetof(isize, width)));
    AS_VERIFY(engine->RegisterObjectProperty("isize", "int height", offsetof(isize, height)));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos& opAddAssign(const isize &in)", SCRIPT_FUNC_THIS(Ipos_AddAssignIsize), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos& opSubAssign(const isize &in)", SCRIPT_FUNC_THIS(Ipos_SubAssignIsize), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos opAdd(const isize &in) const", SCRIPT_FUNC_THIS(Ipos_AddIsize), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "ipos opSub(const isize &in) const", SCRIPT_FUNC_THIS(Ipos_SubIsize), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "bool fitTo(isize size) const", SCRIPT_FUNC_THIS(Ipos_FitToSize), SCRIPT_FUNC_THIS_CONV));

    // Register irect
    REGISTER_HARD_STRONG_TYPE("irect", irect);
    AS_VERIFY(engine->RegisterObjectBehaviour("irect", asBEHAVE_CONSTRUCT, "void f(int x, int y, int width, int height)", SCRIPT_FUNC_THIS(Irect_ConstructXandYandWandH), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("irect", "int x", offsetof(irect, x)));
    AS_VERIFY(engine->RegisterObjectProperty("irect", "int y", offsetof(irect, y)));
    AS_VERIFY(engine->RegisterObjectProperty("irect", "int width", offsetof(irect, width)));
    AS_VERIFY(engine->RegisterObjectProperty("irect", "int height", offsetof(irect, height)));
    AS_VERIFY(engine->RegisterObjectMethod("ipos", "bool fitTo(irect rect) const", SCRIPT_FUNC_THIS(Ipos_FitToRect), SCRIPT_FUNC_THIS_CONV));

    // Register fpos
    REGISTER_HARD_STRONG_TYPE("fpos", fpos);
    AS_VERIFY(engine->RegisterObjectBehaviour("fpos", asBEHAVE_CONSTRUCT, "void f(float x, float y)", SCRIPT_FUNC_THIS(Fpos_ConstructXandY), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("fpos", "float x", offsetof(fpos, x)));
    AS_VERIFY(engine->RegisterObjectProperty("fpos", "float y", offsetof(fpos, y)));

    // Register mpos
    REGISTER_HARD_STRONG_TYPE("mpos", mpos);
    AS_VERIFY(engine->RegisterObjectBehaviour("mpos", asBEHAVE_CONSTRUCT, "void f(int x, int y)", SCRIPT_FUNC_THIS(Mpos_ConstructXandY), SCRIPT_FUNC_THIS_CONV));
    AS_VERIFY(engine->RegisterObjectProperty("mpos", "uint16 x", offsetof(mpos, x)));
    AS_VERIFY(engine->RegisterObjectProperty("mpos", "uint16 y", offsetof(mpos, y)));

    // Register msize
    REGISTER_HARD_STRONG_TYPE("msize", msize);
    AS_VERIFY(engine->RegisterObjectProperty("msize", "uint16 width", offsetof(msize, width)));
    AS_VERIFY(engine->RegisterObjectProperty("msize", "uint16 height", offsetof(msize, height)));
    AS_VERIFY(engine->RegisterObjectMethod("mpos", "bool fitTo(msize size) const", SCRIPT_FUNC_THIS(Mpos_FitToSize), SCRIPT_FUNC_THIS_CONV));

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
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "any GetAsAny(" prop_class_name "Property prop) const", SCRIPT_FUNC_THIS((Property_GetValueAsAny<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "void SetAsAny(" prop_class_name "Property prop, any value)", SCRIPT_FUNC_THIS((Property_SetValueAsAny<real_class>)), SCRIPT_FUNC_THIS_CONV))

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
    AS_VERIFY(engine->RegisterObjectMethod(class_name, IDENT_T_NAME " get_Id() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_SetValue<real_class>)))

#define REGISTER_CUSTOM_ENTITY(class_name, real_class, entity_info) \
    REGISTER_BASE_ENTITY(class_name, real_class); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY(class_name, class_name, real_class); \
    REGISTER_ENTITY_PROPS(class_name, entity_info); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, IDENT_T_NAME " get_Id() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "void Destroy" class_name "(" class_name "@+ entity)", SCRIPT_FUNC_THIS((Game_DestroyOne<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "void Destroy" class_name "s(" class_name "@[]@+ entities)", SCRIPT_FUNC_THIS((Game_DestroyAll<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_set_value_func_ptr.emplace(class_name, SCRIPT_GENERIC((Property_SetValue<real_class>))); \
    entity_is_custom.emplace(class_name)

#define REGISTER_ENTITY_ABSTRACT(class_name, real_class) \
    REGISTER_BASE_ENTITY("Abstract" class_name, Entity); \
    REGISTER_ENTITY_CAST(class_name, real_class, "Abstract" class_name); \
    REGISTER_GETSET_ENTITY("Abstract" class_name, class_name, Entity); \
    AS_VERIFY(engine->RegisterObjectMethod("Abstract" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<Entity>)), SCRIPT_FUNC_THIS_CONV)); \
    entity_get_component_func_ptr.emplace("Abstract" class_name, SCRIPT_GENERIC((Property_GetComponent<Entity>))); \
    entity_get_value_func_ptr.emplace("Abstract" class_name, SCRIPT_GENERIC((Property_GetValue<Entity>))); \
    entity_has_abstract.emplace(class_name)

#define REGISTER_ENTITY_PROTO(class_name, real_class, proto_real_class, entity_info) \
    REGISTER_BASE_ENTITY("Proto" class_name, proto_real_class); \
    REGISTER_ENTITY_CAST("Proto" class_name, proto_real_class, "Entity"); \
    REGISTER_GETSET_ENTITY("Proto" class_name, class_name, proto_real_class); \
    if (entity_has_abstract.count(class_name) != 0) { \
        REGISTER_ENTITY_CAST("Proto" class_name, proto_real_class, "Abstract" class_name); \
    } \
    AS_VERIFY(engine->RegisterObjectMethod("Proto" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<proto_real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, "Proto" class_name "@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    if (entity_is_custom.count(class_name) != 0) { \
        AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "Proto" class_name "@+ GetProto" class_name "(hstring pid)", SCRIPT_FUNC_THIS((Game_GetProtoCustomEntity<entity_info>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("GameSingleton", "Proto" class_name "@[]@ GetProto" class_name "s()", SCRIPT_FUNC_THIS((Game_GetProtoCustomEntities<entity_info>)), SCRIPT_FUNC_THIS_CONV)); \
    } \
    entity_get_component_func_ptr.emplace("Proto" class_name, SCRIPT_GENERIC((Property_GetComponent<proto_real_class>))); \
    entity_get_value_func_ptr.emplace("Proto" class_name, SCRIPT_GENERIC((Property_GetValue<proto_real_class>))); \
    entity_has_protos.emplace(class_name)

#define REGISTER_ENTITY_STATICS(class_name, real_class) \
    REGISTER_BASE_ENTITY("Static" class_name, real_class); \
    REGISTER_ENTITY_CAST("Static" class_name, real_class, "Entity"); \
    REGISTER_GETSET_ENTITY("Static" class_name, class_name, real_class); \
    AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, IDENT_T_NAME " get_StaticId() const", SCRIPT_FUNC_THIS((Entity_Id<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    if (entity_has_abstract.count(class_name) != 0) { \
        REGISTER_ENTITY_CAST("Static" class_name, real_class, "Abstract" class_name); \
    } \
    if (entity_has_protos.count(class_name) != 0) { \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "hstring get_ProtoId() const", SCRIPT_FUNC_THIS((Entity_ProtoId<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
        AS_VERIFY(engine->RegisterObjectMethod("Static" class_name, "Proto" class_name "@+ get_Proto() const", SCRIPT_FUNC_THIS((Entity_Proto<real_class>)), SCRIPT_FUNC_THIS_CONV)); \
    } \
    entity_get_component_func_ptr.emplace("Static" class_name, SCRIPT_GENERIC((Property_GetComponent<real_class>))); \
    entity_get_value_func_ptr.emplace("Static" class_name, SCRIPT_GENERIC((Property_GetValue<real_class>))); \
    entity_has_statics.emplace(class_name)

#define REGISTER_ENTITY_METHOD(class_name, method_decl, func) AS_VERIFY(engine->RegisterObjectMethod(class_name, method_decl, SCRIPT_FUNC_THIS(func), SCRIPT_FUNC_THIS_CONV))

#define REGISTER_ENTITY_HOLDER(holder_class_name, class_name, holder_real_class, real_class, entity_info, entry_name) \
    { \
        const hstring& entry_name_hash = *hashed_strings.emplace(game_engine->ToHashedString(entry_name)).first; \
        if constexpr (SERVER_SCRIPTING) { \
            if (entity_has_protos.count(class_name) != 0) { \
                AS_VERIFY(engine->RegisterObjectMethod(holder_class_name, class_name "@+ Add" entry_name "(hstring pid)", SCRIPT_GENERIC((CustomEntity_Add<holder_real_class, real_class, entity_info>)), SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&entry_name_hash))); \
            } \
            else { \
                AS_VERIFY(engine->RegisterObjectMethod(holder_class_name, class_name "@+ Add" entry_name "()", SCRIPT_GENERIC((CustomEntity_Add<holder_real_class, real_class, entity_info>)), SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&entry_name_hash))); \
            } \
        } \
        AS_VERIFY(engine->RegisterObjectMethod(holder_class_name, class_name "@[]@ Get" entry_name "s()", SCRIPT_GENERIC((CustomEntity_GetAll<holder_real_class, real_class, entity_info>)), SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&entry_name_hash))); \
    }

    REGISTER_BASE_ENTITY("Entity", Entity);

    unordered_set<string> entity_is_custom;
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
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFunc@+, EventPriority priority = EventPriority::Normal)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Subscribe(" entity_name event_name "EventFuncBool@+, EventPriority priority = EventPriority::Normal)", SCRIPT_FUNC_THIS(func_entry##_Subscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFunc@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void Unsubscribe(" entity_name event_name "EventFuncBool@+)", SCRIPT_FUNC_THIS(func_entry##_Unsubscribe), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(entity_name event_name "Event", "void UnsubscribeAll()", SCRIPT_FUNC_THIS(func_entry##_UnsubscribeAll), SCRIPT_FUNC_THIS_CONV)); \
    AS_VERIFY(engine->RegisterObjectMethod(class_name, entity_name event_name "Event@ get_" event_name "()", SCRIPT_FUNC_THIS((Entity_GetSelfForEvent<real_class>)), SCRIPT_FUNC_THIS_CONV))

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
    AS_VERIFY(engine->RegisterObjectType("CritterRemoteCaller", 0, asOBJ_REF | asOBJ_NOHANDLE));
#endif

#if !COMPILER_MODE
#if SERVER_SCRIPTING
#define REMOTE_CALL_RECEIVER_ENTITY static_cast<Player*>(entity)
#else
#define REMOTE_CALL_RECEIVER_ENTITY static_cast<PlayerView*>(entity)
#endif
#else
#define REMOTE_CALL_RECEIVER_ENTITY nullptr
#endif

#define BIND_REMOTE_CALL_RECEIVER(name, func_entry, as_func_decl) \
    RUNTIME_ASSERT(_rpcReceivers.count(name##_hash) == 0); \
    if (auto* func = engine->GetModuleByIndex(0)->GetFunctionByDecl(as_func_decl); func != nullptr) { \
        _rpcReceivers.emplace(name##_hash, [as_func = RefCountHolder(func)]([[maybe_unused]] Entity* entity) { func_entry(REMOTE_CALL_RECEIVER_ENTITY, as_func.get()); }); \
    } \
    else { \
        throw ScriptInitException("Remote call function not found", as_func_decl); \
    }
#endif

    ///@ CodeGen Register

    // Register properties
    for (auto&& [type_name, entity_info] : game_engine->GetEntityTypesInfo()) {
        const auto* registrator = entity_info.PropRegistrator;
        const auto& type_name_str = type_name.as_str();
        const auto is_global = entity_is_global.count(type_name_str) != 0;
        const auto is_has_abstract = entity_has_abstract.count(type_name_str) != 0;
        const auto is_has_protos = entity_info.HasProtos;
        const auto is_has_statics = entity_has_statics.count(type_name_str) != 0;
        const auto class_name = is_global ? type_name_str + "Singleton" : type_name_str;
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
                const auto component_type = strex("{}{}Component", type_name_str, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), get_component_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&component)));
            }
            if (is_has_abstract) {
                const auto component_type = strex("Abstract{}{}Component", type_name_str, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(abstract_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), get_abstract_component_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&component)));
            }
            if (is_has_protos) {
                const auto component_type = strex("Proto{}{}Component", type_name_str, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(proto_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), get_proto_component_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&component)));
            }
            if (is_has_statics) {
                const auto component_type = strex("Static{}{}Component", type_name_str, component).str();
                AS_VERIFY(engine->RegisterObjectType(component_type.c_str(), 0, asOBJ_REF | asOBJ_NOCOUNT));
                AS_VERIFY(engine->RegisterObjectMethod(static_class_name.c_str(), strex("{}@ get_{}() const", component_type, component).c_str(), get_static_component_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(&component)));
            }
        }

        for (const auto i : xrange(registrator->GetPropertiesCount())) {
            const auto* prop = registrator->GetPropertyByIndex(static_cast<int>(i));
            const auto component = prop->GetComponent();
            const auto is_handle = (prop->IsArray() || prop->IsDict());

            if (!prop->IsDisabled()) {
                const auto decl_get = strex("const {}{} get_{}() const", prop->GetFullTypeName(), is_handle ? "@" : "", prop->GetNameWithoutComponent()).str();
                AS_VERIFY(engine->RegisterObjectMethod(component ? strex("{}{}Component", type_name_str, component).c_str() : class_name.c_str(), decl_get.c_str(), get_value_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(prop)));

                if (!prop->IsVirtual() || prop->IsNullGetterForProto()) {
                    if (is_has_abstract) {
                        AS_VERIFY(engine->RegisterObjectMethod(component ? strex("Abstract{}{}Component", type_name_str, component).c_str() : abstract_class_name.c_str(), decl_get.c_str(), get_abstract_value_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(prop)));
                    }
                    if (is_has_protos) {
                        AS_VERIFY(engine->RegisterObjectMethod(component ? strex("Proto{}{}Component", type_name_str, component).c_str() : proto_class_name.c_str(), decl_get.c_str(), get_proto_value_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(prop)));
                    }
                    if (is_has_statics) {
                        AS_VERIFY(engine->RegisterObjectMethod(component ? strex("Static{}{}Component", type_name_str, component).c_str() : static_class_name.c_str(), decl_get.c_str(), get_static_value_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(prop)));
                    }
                }
            }

            if (!prop->IsDisabled() && !prop->IsReadOnly()) {
                const auto decl_set = strex("void set_{}({}{}{})", prop->GetNameWithoutComponent(), is_handle ? "const " : "", prop->GetFullTypeName(), is_handle ? "@+" : "").str();
                AS_VERIFY(engine->RegisterObjectMethod(component ? strex("{}{}Component", type_name_str, component).c_str() : class_name.c_str(), decl_set.c_str(), set_value_func_ptr, SCRIPT_GENERIC_CONV, PASS_AS_PVOID(prop)));
            }
        }

        for (auto&& [group_name, properties] : registrator->GetPropertyGroups()) {
            vector<int> prop_enums;
            prop_enums.reserve(properties.size());
            for (const auto* prop : properties) {
                prop_enums.push_back(prop->GetRegIndex());
            }

            AS_VERIFY(engine->SetDefaultNamespace(strex("{}PropertyGroup", registrator->GetTypeName()).c_str()));
#if !COMPILER_MODE
            const auto it_enum = AngelScriptData->EnumArrays.emplace(MarshalBackArray<int>(engine, strex("{}Property[]", registrator->GetTypeName()).c_str(), prop_enums));
            RUNTIME_ASSERT(it_enum.second);
#endif
            AS_VERIFY(engine->RegisterGlobalFunction(strex("const {}Property[]@+ get_{}()", registrator->GetTypeName(), group_name).c_str(), SCRIPT_GENERIC((Global_Get<CScriptArray*>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(*it_enum.first)));
            AS_VERIFY(engine->SetDefaultNamespace(""));
        }
    }

    AS_VERIFY(engine->RegisterGlobalFunction("GameSingleton@+ get_Game()", SCRIPT_GENERIC((Global_Get<FOEngine*>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine)));

#if SERVER_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ClientCall", 0));
    AS_VERIFY(engine->RegisterObjectProperty("Critter", "CritterRemoteCaller PlayerClientCall", 0));
#elif CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterObjectProperty("Player", "RemoteCaller ServerCall", 0));
#endif

#if CLIENT_SCRIPTING || MAPPER_SCRIPTING
    AS_VERIFY(engine->RegisterGlobalFunction("Map@+ get_CurMap()", SCRIPT_GENERIC((Global_Get<MapView*>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->CurMap)));
#endif
#if CLIENT_SCRIPTING
    AS_VERIFY(engine->RegisterGlobalFunction("Location@+ get_CurLocation()", SCRIPT_GENERIC((Global_Get<LocationView*>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->_curLocation)));
    AS_VERIFY(engine->RegisterGlobalFunction("Player@+ get_CurPlayer()", SCRIPT_GENERIC((Global_Get<PlayerView*>)), SCRIPT_GENERIC_CONV, PTR_OR_DUMMY(_engine->_curPlayer)));
#endif

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
    CompileRootModule(engine, resources);
    engine->ShutDownAndRelease();
#else
#if COMPILER_VALIDATION_MODE
#if SERVER_SCRIPTING
    game_engine->Resources.AddDataSource(strex(App->Settings.BakeOutput).combinePath("ServerAngelScript"), DataSourceType::Default);
#elif CLIENT_SCRIPTING
    game_engine->Resources.AddDataSource(strex(App->Settings.BakeOutput).combinePath("ClientAngelScript"), DataSourceType::Default);
#elif SINGLE_SCRIPTING
    game_engine->Resources.AddDataSource(strex(App->Settings.BakeOutput).combinePath("AngelScript"), DataSourceType::Default);
#elif MAPPER_SCRIPTING
    game_engine->Resources.AddDataSource(strex(App->Settings.BakeOutput).combinePath("MapperAngelScript"), DataSourceType::Default);
#endif
#endif
#if SERVER_SCRIPTING
    File script_file = game_engine->Resources.ReadFile("ServerRootModule.fosb");
#elif CLIENT_SCRIPTING
    File script_file = game_engine->Resources.ReadFile("ClientRootModule.fosb");
#elif SINGLE_SCRIPTING
    File script_file = game_engine->Resources.ReadFile("RootModule.fosb");
#elif MAPPER_SCRIPTING
    File script_file = game_engine->Resources.ReadFile("MapperRootModule.fosb");
#endif
    RUNTIME_ASSERT(script_file);
    RestoreRootModule(engine, {script_file.GetBuf(), script_file.GetSize()});
#endif

#if !COMPILER_MODE || COMPILER_VALIDATION_MODE
    // Index all functions
    {
        RUNTIME_ASSERT(engine->GetModuleCount() == 1);
        auto* mod = engine->GetModuleByIndex(0);

        const auto as_type_to_type_info = [this, engine](int type_id, asDWORD flags, bool is_ret) -> shared_ptr<ScriptTypeInfo> {
            auto* as_type_info = engine->GetTypeInfoById(type_id);
            const auto is_array = as_type_info != nullptr && string_view(as_type_info->GetName()) == "array";

            const auto get_type_name = [engine](int tid) -> string_view {
                switch (tid) {
                case asTYPEID_VOID:
                    return "void";
                case asTYPEID_BOOL:
                    return "bool";
                case asTYPEID_INT8:
                    return "int8";
                case asTYPEID_INT16:
                    return "int16";
                case asTYPEID_INT32:
                    return "int";
                case asTYPEID_INT64:
                    return "int64";
                case asTYPEID_UINT8:
                    return "uint8";
                case asTYPEID_UINT16:
                    return "uint16";
                case asTYPEID_UINT32:
                    return "uint";
                case asTYPEID_UINT64:
                    return "uint64";
                case asTYPEID_FLOAT:
                    return "float";
                case asTYPEID_DOUBLE:
                    return "double";
                default:
                    break;
                }

                auto* ti = engine->GetTypeInfoById(tid);
                RUNTIME_ASSERT(ti);
                string_view name = ti->GetName();

                if (name == "StaticItem") {
                    return "Item";
                }

                return name;
            };

            if (const auto is_ref = (flags & asTM_INOUTREF) != 0; is_ref) {
                if (is_ret) {
                    return nullptr;
                }
                if (is_array) {
                    return nullptr;
                }
                if ((type_id & asTYPEID_OBJHANDLE) != 0) {
                    return nullptr;
                }

                const auto name = get_type_name(type_id);
                const auto it = std::find_if(_engineToScriptType.begin(), _engineToScriptType.end(), [name](const pair<string, shared_ptr<ScriptTypeInfo>>& entry) { //
                    return entry.second->Name == name && entry.second->Accessor->IsPlainData();
                });

                if (it != _engineToScriptType.end()) {
                    return it->second;
                }

                return nullptr;
            }

            if (type_id == asTYPEID_VOID) {
                RUNTIME_ASSERT(is_ret);
                return _engineToScriptType[typeid(void).name()];
            }

            if (is_array) {
                const auto name = get_type_name(as_type_info->GetSubTypeId());
                const auto it = std::find_if(_engineToScriptType.begin(), _engineToScriptType.end(), [name](const pair<string, shared_ptr<ScriptTypeInfo>>& entry) { //
                    return entry.second->Name == name && entry.second->Accessor->IsArray();
                });

                if (it != _engineToScriptType.end()) {
                    return it->second;
                }

                return nullptr;
            }

            const auto name = get_type_name(type_id);
            const auto it = std::find_if(_engineToScriptType.begin(), _engineToScriptType.end(), [name](const pair<string, shared_ptr<ScriptTypeInfo>>& entry) { //
                return entry.second->Name == name && !entry.second->Accessor->IsArray();
            });

            if (it != _engineToScriptType.end()) {
                return it->second;
            }

            return nullptr;
        };

        for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
            auto* func = mod->GetFunctionByIndex(i);

            // Bind
            const auto func_name = GetASFuncName(func, *game_engine);
            const auto it = _funcMap.emplace(func_name, ScriptFuncDesc());
            auto& func_desc = it->second;

            func_desc.Name = func_name;
            func_desc.Declaration = func->GetDeclaration(true, true, true);

#if !COMPILER_VALIDATION_MODE
            func_desc.Call = [script_sys = AngelScriptData.get(), gen = &func_desc, func](initializer_list<void*> args, void* ret) noexcept { //
                return AngelScriptFuncCall(script_sys, gen, func, args, ret);
            };
#endif

            for (asUINT p = 0; p < func->GetParamCount(); p++) {
                int param_type_id;
                asDWORD param_flags;
                AS_VERIFY(func->GetParam(p, &param_type_id, &param_flags));

                func_desc.ArgsType.emplace_back(as_type_to_type_info(param_type_id, param_flags, false));
            }

            asDWORD ret_flags = 0;
            int ret_type_id = func->GetReturnTypeId(&ret_flags);
            func_desc.RetType = as_type_to_type_info(ret_type_id, ret_flags, true);

            func_desc.CallSupported = func_desc.RetType && std::find(func_desc.ArgsType.begin(), func_desc.ArgsType.end(), nullptr) == func_desc.ArgsType.end();

            // Check for special module init function
            if (func_desc.ArgsType.empty() && func_desc.RetType && func_desc.RetType->Name == "void") {
                RUNTIME_ASSERT(func_desc.CallSupported);
                const auto func_name_ex = strex(func->GetName());

                if (func_name_ex.compareIgnoreCase("ModuleInit") || func_name_ex.compareIgnoreCase("module_init")) {
                    _initFunc.push_back(&func_desc);
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
}

class BinaryStream : public asIBinaryStream
{
public:
    explicit BinaryStream(vector<asBYTE>& buf) :
        _binBuf {buf}
    {
    }

    void Write(const void* ptr, asUINT size) override
    {
        if (!ptr || size == 0) {
            return;
        }

        _binBuf.resize(_binBuf.size() + size);
        MemCopy(&_binBuf[_writePos], ptr, size);
        _writePos += size;
    }

    void Read(void* ptr, asUINT size) override
    {
        if (!ptr || size == 0) {
            return;
        }

        MemCopy(ptr, &_binBuf[_readPos], size);
        _readPos += size;
    }

    auto GetBuf() -> vector<asBYTE>& { return _binBuf; }

private:
    vector<asBYTE>& _binBuf;
    size_t _readPos {};
    size_t _writePos {};
};

#if COMPILER_MODE && !COMPILER_VALIDATION_MODE
static void CompileRootModule(asIScriptEngine* engine, const FileSystem& resources)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(engine->GetModuleCount() == 0);

    // File loader
    class ScriptLoader : public Preprocessor::FileLoader
    {
    public:
        ScriptLoader(const string* root, const map<string, string>* files) :
            _rootScript {root},
            _scriptFiles {files}
        {
            STACK_TRACE_ENTRY();

            //
        }

        auto LoadFile(const std::string& dir, const std::string& file_name, std::vector<char>& data, std::string& file_path) -> bool override
        {
            STACK_TRACE_ENTRY();

            if (_rootScript != nullptr) {
                data.resize(_rootScript->size());
                MemCopy(data.data(), _rootScript->data(), _rootScript->size());
                _rootScript = nullptr;
                file_path = "(Root)";
                return true;
            }

            _includeDeep++;

            file_path = file_name;
            data.resize(0);

            if (_includeDeep == 1) {
                const auto it = _scriptFiles->find(string(file_name));
                RUNTIME_ASSERT(it != _scriptFiles->end());

                data.resize(it->second.size());
                MemCopy(data.data(), it->second.data(), it->second.size());
                return true;
            }

            return Preprocessor::FileLoader::LoadFile(dir, file_name, data, file_path);
        }

        void FileLoaded() override
        {
            STACK_TRACE_ENTRY();

            _includeDeep--;
        }

    private:
        const string* _rootScript;
        const map<string, string>* _scriptFiles;
        int _includeDeep {};
    };

    auto script_files = resources.FilterFiles("fos");

    map<string, string> final_script_files;
    vector<tuple<int, string, string>> final_script_files_order;

    while (script_files.MoveNext()) {
        auto script_file = script_files.GetCurFile();
        string script_name = string(script_file.GetName());
        string script_path = string(script_file.GetFullPath());
        string script_content = script_file.GetStr();

        const auto line_sep = script_content.find('\n');
        const auto first_line = script_content.substr(0, line_sep);

        int sort = 0;
        const auto sort_pos = first_line.find("Sort ");

        if (sort_pos != string::npos) {
            sort = strex(first_line.substr(sort_pos + "Sort "_len)).substringUntil(' ').toInt();
        }

        final_script_files_order.push_back(std::make_tuple(sort, script_name, script_path));
        final_script_files.emplace(script_path, std::move(script_content));
    }

    std::stable_sort(final_script_files_order.begin(), final_script_files_order.end(), [](auto& a, auto& b) {
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
        root_script.append("#include \"");
        root_script.append(script_path);
        root_script.append("\"\n");
    }

    Preprocessor::UndefAll();
    Preprocessor::Define("SERVER 0");
    Preprocessor::Define("CLIENT 0");
    Preprocessor::Define("MAPPER 0");
    Preprocessor::Define("SINGLE 0");
#if SERVER_SCRIPTING
    Preprocessor::Define("SERVER 1");
#elif CLIENT_SCRIPTING
    Preprocessor::Define("CLIENT 1");
#elif SINGLE_SCRIPTING
    Preprocessor::Define("SINGLE 1");
    Preprocessor::Define("SERVER 1");
    Preprocessor::Define("CLIENT 1");
#elif MAPPER_SCRIPTING
    Preprocessor::Define("MAPPER 1");
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
    std::vector<uint8> lnt_data;
    Preprocessor::StoreLineNumberTranslator(lnt, lnt_data);

    vector<uint8> data;
    auto writer = DataWriter(data);
    writer.Write<uint>(static_cast<uint>(buf.size()));
    writer.WritePtr(buf.data(), buf.size());
    writer.Write<uint>(static_cast<uint>(lnt_data.size()));
    writer.WritePtr(lnt_data.data(), lnt_data.size());

#if SERVER_SCRIPTING
    const string script_out_path = strex(App->Settings.BakeOutput).combinePath("ServerAngelScript/ServerRootModule.fosb");
#elif CLIENT_SCRIPTING
    const string script_out_path = strex(App->Settings.BakeOutput).combinePath("ClientAngelScript/ClientRootModule.fosb");
#elif SINGLE_SCRIPTING
    const string script_out_path = strex(App->Settings.BakeOutput).combinePath("SingleAngelScript/SingleRootModule.fosb");
#elif MAPPER_SCRIPTING
    const string script_out_path = strex(App->Settings.BakeOutput).combinePath("MapperAngelScript/MapperRootModule.fosb");
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
static void RestoreRootModule(asIScriptEngine* engine, const_span<uint8> script_bin)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(engine->GetModuleCount() == 0);
    RUNTIME_ASSERT(!script_bin.empty());

    auto reader = DataReader({script_bin.data(), script_bin.size()});
    vector<asBYTE> buf(reader.Read<uint>());
    MemCopy(buf.data(), reader.ReadPtr<asBYTE>(buf.size()), buf.size());
    std::vector<uint8> lnt_data(reader.Read<uint>());
    MemCopy(lnt_data.data(), reader.ReadPtr<uint8>(lnt_data.size()), lnt_data.size());
    reader.VerifyEnd();
    RUNTIME_ASSERT(!buf.empty());
    RUNTIME_ASSERT(!lnt_data.empty());

    asIScriptModule* mod = engine->GetModule("Root", asGM_ALWAYS_CREATE);
    if (mod == nullptr) {
        throw ScriptException("Create root module fail");
    }

    auto* lnt = Preprocessor::RestoreLineNumberTranslator(lnt_data);
    engine->SetUserData(lnt, 5);

    BinaryStream binary {buf};
    int as_result = mod->LoadByteCode(&binary);
    if (as_result < 0) {
        throw ScriptException("Can't load binary", as_result);
    }
}
#endif
