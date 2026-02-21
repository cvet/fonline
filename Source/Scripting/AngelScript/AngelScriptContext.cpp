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

#include "AngelScriptContext.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBackend.h"
#include "AngelScriptHelpers.h"

#include <as_context.h>
#include <preprocessor.h>
// ReSharper disable CppRedundantQualifier

#include "WinApiUndef-Include.h" // Remove garbage from includes above

FO_BEGIN_NAMESPACE

struct AngelScriptContextExtendedData
{
    bool ExecutionActive {};
    size_t ExecutionCalls {};
    FO_NAMESPACE nanotime SuspendEndTime {};
    FO_NAMESPACE vector<const FO_NAMESPACE SourceLocationData*> StackTrace {};
    FO_NAMESPACE vector<FO_NAMESPACE refcount_ptr<const FO_NAMESPACE Entity>> ValidCheck {};
#if FO_TRACY
    FO_NAMESPACE vector<TracyCZoneCtx> TracyExecutionCalls {};
#endif
};

struct StackTraceEntryStorage
{
    static constexpr size_t STACK_TRACE_FUNC_BUF_SIZE = 64;
    static constexpr size_t STACK_TRACE_FILE_BUF_SIZE = 128;

    array<char, STACK_TRACE_FUNC_BUF_SIZE> FuncBuf {};
    size_t FuncBufLen {};
    array<char, STACK_TRACE_FILE_BUF_SIZE> FileBuf {};
    size_t FileBufLen {};

    SourceLocationData SrcLoc {};
};

struct AngelScriptStackTraceData
{
    unordered_map<size_t, StackTraceEntryStorage> ScriptCallCacheEntries {};
    list<StackTraceEntryStorage> ScriptCallExceptionEntries {};
    std::mutex ScriptCallCacheEntriesLocker {};
};
FO_GLOBAL_DATA(AngelScriptStackTraceData, AngelScriptStackTrace);

static void AngelScriptBeginCall(AngelScript::asIScriptContext* ctx, AngelScript::asIScriptFunction* func, size_t program_pos);
static void AngelScriptEndCall(AngelScript::asIScriptContext* ctx) noexcept;
static void AngelScriptException(AngelScript::asIScriptContext* ctx, void* param);

static void CleanupScriptContext(AngelScript::asIScriptContext* ctx)
{
    FO_STACK_TRACE_ENTRY();

    const auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
    delete ctx_ext;
}

AngelScriptContextManager::AngelScriptContextManager(AngelScript::asIScriptEngine* as_engine, timespan overrun_timeout) :
    _asEngine {as_engine},
    _overrunTimeout {overrun_timeout}
{
    FO_STACK_TRACE_ENTRY();

    as_engine->SetContextUserDataCleanupCallback(CleanupScriptContext);
}

void AngelScriptContextManager::CreateContext()
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = _asEngine->CreateContext();
    FO_RUNTIME_ASSERT(ctx);
    vec_add_unique_value(_freeContexts, ctx);

    auto ctx_ext = SafeAlloc::MakeUnique<AngelScriptContextExtendedData>();
    ctx_ext->StackTrace.reserve(128);
    ctx->SetUserData(cast_to_void(ctx_ext.release()));

#if !FO_NO_MANUAL_STACK_TRACE
    auto* ctx_impl = dynamic_cast<AngelScript::asCContext*>(ctx);
    FO_RUNTIME_ASSERT(ctx_impl);
    ctx_impl->BeginScriptCall = AngelScriptBeginCall;
    ctx_impl->EndScriptCall = AngelScriptEndCall;
#endif

    int32 as_result = 0;
    FO_AS_VERIFY(ctx->SetExceptionCallback(asFUNCTION(AngelScriptException), nullptr, AngelScript::asCALL_CDECL));
}

auto AngelScriptContextManager::RequestContext() -> AngelScript::asIScriptContext*
{
    FO_STACK_TRACE_ENTRY();

    if (_freeContexts.empty()) {
        CreateContext();
    }

    auto ctx = _freeContexts.back();
    _freeContexts.pop_back();
    vec_add_unique_value(_busyContexts, ctx);
    return ctx.get();
}

void AngelScriptContextManager::ReturnContext(AngelScript::asIScriptContext* ctx) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->GetState() == AngelScript::asEXECUTION_SUSPENDED) {
        return;
    }

    try {
        FO_RUNTIME_ASSERT(ctx->GetState() != AngelScript::asEXECUTION_ACTIVE);

        int32 as_result = 0;
        FO_AS_VERIFY(ctx->Unprepare());

        refcount_ptr ctx_holder = ctx;
        vec_remove_unique_value(_busyContexts, ctx);
        vec_add_unique_value(_freeContexts, ctx);

        auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
        FO_RUNTIME_ASSERT(ctx_ext);
        ctx_ext->SuspendEndTime = {};
        ctx_ext->ValidCheck.clear();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
        (void)ctx->Release();
    }
}

auto AngelScriptContextManager::PrepareContext(AngelScript::asIScriptFunction* func) -> AngelScript::asIScriptContext*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(func);

    auto* ctx = RequestContext();
    const auto as_result = ctx->Prepare(func);

    if (as_result < 0) {
        ReturnContext(ctx);
        throw ScriptCallException("Can't prepare context", func->GetName(), as_result);
    }

    return ctx;
}

void AngelScriptContextManager::AddSetupScriptContextEntity(AngelScript::asIScriptContext* ctx, Entity* entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
    FO_RUNTIME_ASSERT(ctx_ext);

    vec_safe_add_unique_value(ctx_ext->ValidCheck, entity);
}

auto AngelScriptContextManager::RunContext(AngelScript::asIScriptContext* ctx, bool can_suspend) -> bool
{
    FO_STACK_TRACE_ENTRY();

    int32 exec_result = 0;

    {
        auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
        FO_RUNTIME_ASSERT(ctx_ext);
        FO_RUNTIME_ASSERT(!ctx_ext->ExecutionActive);
        FO_RUNTIME_ASSERT(!ctx_ext->ExecutionCalls);

        ctx_ext->ExecutionActive = true;

        if (ctx->GetState() == AngelScript::asEXECUTION_SUSPENDED) {
            for (const auto* loc : ctx_ext->StackTrace) {
                ctx_ext->ExecutionCalls++;
                PushStackTrace(loc);

#if FO_TRACY
                const auto loc_func = string_view(loc->function);
                const auto loc_file = string_view(loc->file);
                const auto tracy_srcloc = ___tracy_alloc_srcloc(loc->line, loc_file.data(), loc_file.length(), loc_func.data(), loc_func.length(), 0);
                const auto tracy_ctx = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
                ctx_ext->TracyExecutionCalls.emplace_back(tracy_ctx);
#endif
            }
        }

        auto after_execution = scope_exit([&]() noexcept {
            ctx_ext->ExecutionActive = false;

            while (ctx_ext->ExecutionCalls > 0) {
                ctx_ext->ExecutionCalls--;
                PopStackTrace();
            }

            if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
                ctx_ext->StackTrace.clear();
            }

#if FO_TRACY
            while (!ctx_ext->TracyExecutionCalls.empty()) {
                ___tracy_emit_zone_end(ctx_ext->TracyExecutionCalls.back());
                ctx_ext->TracyExecutionCalls.pop_back();
            }
#endif
        });

        const auto execution_time = TimeMeter();

        try {
            exec_result = ctx->Execute();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            ctx->SetException(ex.what());
            exec_result = AngelScript::asEXECUTION_EXCEPTION;
        }

        const auto execution_duration = execution_time.GetDuration();

        if (execution_duration >= _overrunTimeout && !IsRunInDebugger()) {
            if constexpr (!FO_DEBUG) {
                const string func_decl = ctx->GetFunction()->GetDeclaration(true, true);
                WriteLog("Script execution overrun: {} ({})", func_decl, execution_duration);
            }
        }
    }

    if (exec_result == AngelScript::asEXECUTION_SUSPENDED) {
        if (!can_suspend) {
            ctx->Abort();
            throw ScriptCallException("Can't yield current routine");
        }

        return false;
    }

    if (exec_result != AngelScript::asEXECUTION_FINISHED) {
        if (exec_result == AngelScript::asEXECUTION_EXCEPTION) {
            ctx->Abort();
            const auto ex = ctx->GetStdException();

            if (ex) {
                std::rethrow_exception(ex);
            }
            else {
                const string ex_string = ctx->GetExceptionString();
                throw ScriptCallException(ex_string);
            }
        }

        if (exec_result == AngelScript::asEXECUTION_ABORTED) {
            throw ScriptCallException("Execution of script aborted");
        }

        ctx->Abort();
        throw ScriptCallException("Context execution error", exec_result);
    }

    return true;
}

void AngelScriptContextManager::SuspendScriptContext(AngelScript::asIScriptContext* ctx, nanotime time)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
        ctx->Suspend();
    }

    auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
    FO_RUNTIME_ASSERT(ctx_ext);

    ctx_ext->SuspendEndTime = time;
}

void AngelScriptContextManager::ResumeSuspendedContexts(nanotime time)
{
    FO_STACK_TRACE_ENTRY();

    if (_busyContexts.empty()) {
        return;
    }

    vector<AngelScript::asIScriptContext*> resume_contexts;
    vector<AngelScript::asIScriptContext*> finish_contexts;

    for (auto& ctx : _busyContexts) {
        if (ctx->GetState() == AngelScript::asEXECUTION_PREPARED || ctx->GetState() == AngelScript::asEXECUTION_SUSPENDED) {
            const auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
            FO_RUNTIME_ASSERT(ctx_ext);

            if (time >= ctx_ext->SuspendEndTime) {
                const bool some_entity_destroyed = std::ranges::any_of(ctx_ext->ValidCheck, [](auto&& e) { return e->IsDestroyed(); });

                if (some_entity_destroyed) {
                    finish_contexts.emplace_back(ctx.get());
                }
                else {
                    resume_contexts.emplace_back(ctx.get());
                }
            }
        }
        else {
            BreakIntoDebugger();
            finish_contexts.emplace_back(ctx.get());
        }
    }

    for (auto* ctx : resume_contexts) {
        try {
            auto return_ctx = scope_exit([&]() noexcept { ReturnContext(ctx); });
            RunContext(ctx, true);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }

    for (auto* ctx : finish_contexts) {
        ReturnContext(ctx);
    }
}

static void AngelScriptBeginCall(AngelScript::asIScriptContext* ctx, AngelScript::asIScriptFunction* func, size_t program_pos)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());

    if (ctx_ext == nullptr || !ctx_ext->ExecutionActive) {
        return;
    }

    StackTraceEntryStorage* storage = nullptr;

    {
        std::scoped_lock lock {AngelScriptStackTrace->ScriptCallCacheEntriesLocker};

        const auto it = AngelScriptStackTrace->ScriptCallCacheEntries.find(program_pos);

        if (it != AngelScriptStackTrace->ScriptCallCacheEntries.end()) {
            storage = &it->second;
        }
    }

    if (storage == nullptr) {
        const int32 ctx_line = ctx->GetLineNumber();

        const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
        const auto& orig_file = Preprocessor::ResolveOriginalFile(ctx_line, lnt);
        const auto orig_line = numeric_cast<uint32>(Preprocessor::ResolveOriginalLine(ctx_line, lnt));

        const auto* func_decl = func->GetDeclaration(true);

        {
            std::scoped_lock lock {AngelScriptStackTrace->ScriptCallCacheEntriesLocker};

            storage = &AngelScriptStackTrace->ScriptCallCacheEntries.emplace(program_pos, StackTraceEntryStorage {}).first->second;

            const auto safe_copy = [](auto& to, size_t& len, string_view from) {
                len = std::min(from.length(), to.size() - 1);
                MemCopy(to.data(), from.data(), len);
                to[len] = 0;
            };

            safe_copy(storage->FuncBuf, storage->FuncBufLen, func_decl);
            safe_copy(storage->FileBuf, storage->FileBufLen, orig_file);

            storage->SrcLoc.name = nullptr;
            storage->SrcLoc.function = storage->FuncBuf.data();
            storage->SrcLoc.file = storage->FileBuf.data();
            storage->SrcLoc.line = orig_line;
        }
    }

    ctx_ext->ExecutionCalls++;
    ctx_ext->StackTrace.emplace_back(&storage->SrcLoc);
    PushStackTrace(&storage->SrcLoc);

#if FO_TRACY
    const auto tracy_srcloc = ___tracy_alloc_srcloc(storage->SrcLoc.line, storage->FileBuf.data(), storage->FileBufLen, storage->FuncBuf.data(), storage->FuncBufLen, 0);
    const auto tracy_ctx = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
    ctx_ext->TracyExecutionCalls.emplace_back(tracy_ctx);
#endif
}

static void AngelScriptEndCall(AngelScript::asIScriptContext* ctx) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx_ext = cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());

    if (ctx_ext == nullptr || !ctx_ext->ExecutionActive) {
        return;
    }

    FO_STRONG_ASSERT(ctx_ext->ExecutionCalls > 0);

    if (ctx_ext->ExecutionCalls > 0) {
        ctx_ext->ExecutionCalls--;
        ctx_ext->StackTrace.pop_back();
        PopStackTrace();

#if FO_TRACY
        ___tracy_emit_zone_end(ctx_ext->TracyExecutionCalls.back());
        ctx_ext->TracyExecutionCalls.pop_back();
#endif
    }
}

static void AngelScriptException(AngelScript::asIScriptContext* ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(param);

    auto& ex = ctx->GetStdException();

    if (ex) {
        return;
    }

    const auto* ex_func = ctx->GetExceptionFunction();
    const auto ex_line = ctx->GetExceptionLineNumber();

    const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
    const auto& ex_orig_file = Preprocessor::ResolveOriginalFile(ex_line, lnt);
    const auto ex_orig_line = numeric_cast<uint32>(Preprocessor::ResolveOriginalLine(ex_line, lnt));

    const auto* func_decl = ex_func->GetDeclaration(true);

    StackTraceEntryStorage* storage = nullptr;

    {
        std::scoped_lock lock {AngelScriptStackTrace->ScriptCallCacheEntriesLocker};

        storage = &AngelScriptStackTrace->ScriptCallExceptionEntries.emplace_back(StackTraceEntryStorage {});

        const auto safe_copy = [](auto& to, size_t& len, string_view from) {
            len = std::min(from.length(), to.size() - 1);
            MemCopy(to.data(), from.data(), len);
            to[len] = 0;
        };

        safe_copy(storage->FuncBuf, storage->FuncBufLen, func_decl);
        safe_copy(storage->FileBuf, storage->FileBufLen, ex_orig_file);

        storage->SrcLoc.name = nullptr;
        storage->SrcLoc.function = storage->FuncBuf.data();
        storage->SrcLoc.file = storage->FileBuf.data();
        storage->SrcLoc.line = ex_orig_line;
    }

    PushStackTrace(&storage->SrcLoc);
    auto stack_trace_entry_end = scope_exit([]() noexcept { PopStackTrace(); });
    ex = std::make_exception_ptr(ScriptException(ctx->GetExceptionString()));
}

FO_END_NAMESPACE

#endif
