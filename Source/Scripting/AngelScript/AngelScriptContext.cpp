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

#include <angelscript.h>
#include <preprocessor.h>

#if FO_TRACY
#include <as_context.h> // For AngelScript::asCContext::BeginScriptCall / EndScriptCall hooks
#endif
// ReSharper disable CppRedundantQualifier

#include "WinApiUndef-Include.h" // Remove garbage from includes above

FO_BEGIN_NAMESPACE

static auto BuildScriptFrameForContext(AngelScript::asIScriptContext* ctx, uint32_t stack_level) noexcept -> optional<StackTraceFrame>;
static void CollectScriptStackLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept;

struct AngelScriptStackTraceInstaller
{
    AngelScriptStackTraceInstaller() noexcept { SetScriptStackTraceProvider(&CollectScriptStackLayers); }
};
FO_GLOBAL_DATA(AngelScriptStackTraceInstaller, AngelScriptStackTraceInstall);

static void AngelScriptTranslateAppException(AngelScript::asIScriptContext* ctx, void* param);
static void AngelScriptException(AngelScript::asIScriptContext* ctx, void* param);

#if FO_TRACY
struct AngelScriptTracyCallEntry
{
    static constexpr size_t FUNC_BUF_SIZE = 64;
    static constexpr size_t FILE_BUF_SIZE = 128;

    array<char, FUNC_BUF_SIZE> FuncBuf {};
    size_t FuncBufLen {};
    array<char, FILE_BUF_SIZE> FileBuf {};
    size_t FileBufLen {};
    uint32_t Line {};
};

struct AngelScriptTracyData
{
    mutex CacheLocker {};
    unordered_map<size_t, AngelScriptTracyCallEntry> CacheEntries FO_TSA_GUARDED_BY(CacheLocker) {};
};
FO_GLOBAL_DATA(AngelScriptTracyData, AngelScriptTracy);

static void AngelScriptBeginCall(AngelScript::asIScriptContext* ctx, AngelScript::asIScriptFunction* func);
static void AngelScriptEndCall(AngelScript::asIScriptContext* ctx) noexcept;
#endif

static void CleanupScriptContext(AngelScript::asIScriptContext* ctx)
{
    FO_STACK_TRACE_ENTRY();

    const auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    delete ctx_ext;
}

auto AngelScriptContextExtendedData::Get(AngelScript::asIScriptContext* ctx) -> AngelScriptContextExtendedData*
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
}

auto AngelScriptContextExtendedData::Get(const AngelScript::asIScriptContext* ctx) -> const AngelScriptContextExtendedData*
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<const AngelScriptContextExtendedData*>(ctx->GetUserData());
}

AngelScriptContextManager::AngelScriptContextManager(AngelScript::asIScriptEngine* as_engine, timespan overrun_timeout, function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> debugger_stop_callback) :
    _asEngine {as_engine},
    _overrunTimeout {overrun_timeout},
    _debuggerStopCallback {std::move(debugger_stop_callback)}
{
    FO_STACK_TRACE_ENTRY();

    as_engine->SetContextUserDataCleanupCallback(CleanupScriptContext);

    int32_t as_result = 0;
    FO_AS_VERIFY(as_engine->SetTranslateAppExceptionCallback(asFUNCTION(AngelScriptTranslateAppException), nullptr, AngelScript::asCALL_CDECL));
}

AngelScriptContextManager::~AngelScriptContextManager()
{
    FO_STACK_TRACE_ENTRY();

    const auto cleanup_context = [](AngelScript::asIScriptContext* ctx) {
        safe_call([&] {
            auto state = ctx->GetState();

            if (state == AngelScript::asEXECUTION_ACTIVE || state == AngelScript::asEXECUTION_SUSPENDED) {
                ctx->Abort();
                state = ctx->GetState();
            }

            if (state != AngelScript::asEXECUTION_UNINITIALIZED) {
                ctx->Unprepare();
            }
        });
    };

    for (const auto& ctx : _busyContexts) {
        cleanup_context(ctx.get_no_const());
    }
    for (const auto& ctx : _freeContexts) {
        cleanup_context(ctx.get_no_const());
    }
}

void AngelScriptContextManager::CreateContext()
{
    FO_STACK_TRACE_ENTRY();

    auto* ctx = _asEngine->CreateContext();
    FO_RUNTIME_ASSERT(ctx);
    _freeContexts.emplace_back(refcount_ptr<AngelScript::asIScriptContext>::adopt, ctx);

    auto ctx_ext = SafeAlloc::MakeUnique<AngelScriptContextExtendedData>();
#if FO_TRACY
    ctx_ext->TracyStackTrace.reserve(128);
    ctx_ext->TracyZones.reserve(128);
#endif
    ctx->SetUserData(cast_to_void(ctx_ext.release()));

#if FO_TRACY
    auto* ctx_impl = dynamic_cast<AngelScript::asCContext*>(ctx);
    FO_RUNTIME_ASSERT(ctx_impl);
    ctx_impl->BeginScriptCall = AngelScriptBeginCall;
    ctx_impl->EndScriptCall = AngelScriptEndCall;
#endif

    int32_t as_result = 0;
    FO_AS_VERIFY(ctx->SetExceptionCallback(asFUNCTION(AngelScriptException), nullptr, AngelScript::asCALL_CDECL));

    if (_contextSetupCallback) {
        _contextSetupCallback(ctx, AngelScriptContextSetupReason::Create);
    }
}

auto AngelScriptContextManager::RequestContext() -> AngelScript::asIScriptContext*
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_poolLocker};

    if (_freeContexts.empty()) {
        CreateContext();
    }

    auto ctx = _freeContexts.back();
    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx.get());
    FO_RUNTIME_ASSERT(ctx_ext);

    _freeContexts.pop_back();
    vec_add_unique_value(_busyContexts, ctx);

    auto* parent_ctx = AngelScript::asGetActiveContext();
    auto* parent_ctx_ext = parent_ctx != nullptr ? AngelScriptContextExtendedData::Get(parent_ctx) : nullptr;
    auto* root_ctx = parent_ctx_ext != nullptr ? parent_ctx_ext->Root.get() : parent_ctx;
    ctx_ext->Parent = parent_ctx;
    ctx_ext->Root = root_ctx != nullptr ? root_ctx : ctx.get();
    ctx_ext->Exception = {};

    CaptureNativeStackFrames(ctx_ext->BirthNativeFrames, ctx_ext->BirthNativeFrameCount, ctx_ext->BirthNativeTruncated, 1);

    if (_contextSetupCallback) {
        _contextSetupCallback(ctx.get(), AngelScriptContextSetupReason::Request);
    }

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

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->Unprepare());

        scoped_lock lock {_poolLocker};

        refcount_ptr ctx_holder = ctx;
        vec_remove_unique_value(_busyContexts, ctx);
        vec_add_unique_value(_freeContexts, ctx);

        auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
        FO_RUNTIME_ASSERT(ctx_ext);

        if (_contextSetupCallback) {
            _contextSetupCallback(ctx, AngelScriptContextSetupReason::Return);
        }

        ctx_ext->DeferredPropertyEntity = nullptr;
        ctx_ext->DeferredProperty = nullptr;
        ctx_ext->DeferredPropertyCallback = nullptr;
        ctx_ext->Parent = nullptr;
        ctx_ext->Root = nullptr;
        ctx_ext->Exception = {};
        ctx_ext->BirthNativeFrameCount = 0;
        ctx_ext->BirthNativeTruncated = false;

        for (auto& other : _busyContexts) {
            auto* other_ext = AngelScriptContextExtendedData::Get(other.get());
            FO_RUNTIME_ASSERT(other_ext);

            if (other_ext->Parent == ctx) {
                other_ext->Parent.reset();
            }
            if (other_ext->Root == ctx) {
                other_ext->Root.reset();
            }
        }
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

void AngelScriptContextManager::SetContextSetupCallback(function<void(AngelScript::asIScriptContext*, AngelScriptContextSetupReason)> context_setup_callback)
{
    FO_STACK_TRACE_ENTRY();

    _contextSetupCallback = std::move(context_setup_callback);
}

auto AngelScriptContextManager::IsDeferredPropertySetterScheduled(const Entity* entity, const Property* prop, AngelScript::asIScriptFunction* func) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    scoped_lock lock {_poolLocker};

    for (const auto& ctx : _busyContexts) {
        const auto* ctx_ext = AngelScriptContextExtendedData::Get(const_cast<AngelScript::asIScriptContext*>(ctx.get()));
        FO_RUNTIME_ASSERT(ctx_ext);

        if (ctx_ext->DeferredPropertyEntity == entity && ctx_ext->DeferredProperty == prop && ctx_ext->DeferredPropertyCallback == func) {
            return true;
        }
    }

    return false;
}

void AngelScriptContextManager::MarkDeferredPropertySetter(AngelScript::asIScriptContext* ctx, const Entity* entity, const Property* prop, AngelScript::asIScriptFunction* func)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_RUNTIME_ASSERT(ctx_ext);

    ctx_ext->DeferredPropertyEntity = entity;
    ctx_ext->DeferredProperty = prop;
    ctx_ext->DeferredPropertyCallback = func;
}

auto AngelScriptContextManager::RunContext(AngelScript::asIScriptContext* ctx, bool can_suspend) -> bool
{
    FO_STACK_TRACE_ENTRY();

    int32_t exec_result = 0;

    {
        auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
        FO_RUNTIME_ASSERT(ctx_ext);

#if FO_TRACY
        FO_RUNTIME_ASSERT(!ctx_ext->TracyExecutionActive);
        FO_RUNTIME_ASSERT(!ctx_ext->TracyExecutionCalls);
        ctx_ext->TracyExecutionActive = true;

        if (ctx->GetState() == AngelScript::asEXECUTION_SUSPENDED) {
            for (const auto* entry : ctx_ext->TracyStackTrace) {
                ctx_ext->TracyExecutionCalls++;
                const auto tracy_srcloc = ___tracy_alloc_srcloc(entry->Line, entry->FileBuf.data(), entry->FileBufLen, entry->FuncBuf.data(), entry->FuncBufLen, 0);
                const auto tracy_zone = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
                ctx_ext->TracyZones.emplace_back(tracy_zone);
            }
        }

        auto tracy_after_execution = scope_exit([&]() noexcept {
            ctx_ext->TracyExecutionActive = false;

            while (ctx_ext->TracyExecutionCalls > 0) {
                ctx_ext->TracyExecutionCalls--;
                ___tracy_emit_zone_end(ctx_ext->TracyZones.back());
                ctx_ext->TracyZones.pop_back();
            }

            if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
                ctx_ext->TracyStackTrace.clear();
            }
        });
#endif

        const auto execution_time = TimeMeter();

        try {
            exec_result = ctx->Execute();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            ctx->SetException(ex.what());
            exec_result = AngelScript::asEXECUTION_EXCEPTION;
        }

        if (_overrunTimeout) {
            const auto execution_duration = execution_time.GetDuration();

            if (execution_duration >= _overrunTimeout && !IsRunInDebugger()) {
                if constexpr (!FO_DEBUG) {
                    const string func_decl = ctx->GetFunction()->GetDeclaration(true, true);
                    WriteLog("Script execution overrun: {} ({})", func_decl, execution_duration);
                }
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
            const string ex_string = ctx->GetExceptionString();
            const auto ex = AngelScriptContextExtendedData::Get(ctx)->Exception;

            if (_debuggerStopCallback) {
                string source_path;
                optional<uint32_t> source_line;
                string function_name;

                if (const auto* ex_func = ctx->GetExceptionFunction(); ex_func != nullptr) {
                    function_name = ex_func->GetName();
                }

                if (const auto ex_line = ctx->GetExceptionLineNumber(); ex_line != 0) {
                    const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
                    const auto& ex_orig_file = Preprocessor::ResolveOriginalFile(ex_line, lnt);
                    const auto ex_orig_line = Preprocessor::ResolveOriginalLine(ex_line, lnt);

                    source_path = ex_orig_file;

                    if (ex_orig_line != 0) {
                        source_line = numeric_cast<uint32_t>(ex_orig_line - 1);
                    }
                }

                _debuggerStopCallback("exception", ex_string, source_path, source_line, function_name);
            }

            ctx->Abort();

            if (ex) {
                std::rethrow_exception(ex);
            }
            else {
                throw ScriptCallException(ex_string);
            }
        }

        if (exec_result == AngelScript::asEXECUTION_ABORTED) {
            if (_debuggerStopCallback) {
                _debuggerStopCallback("aborted", "Execution of script aborted", "", std::nullopt, "");
            }

            throw ScriptCallException("Execution of script aborted");
        }

        if (_debuggerStopCallback) {
            _debuggerStopCallback("error", strex("Context execution error: {}", exec_result).str(), "", std::nullopt, "");
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

    FO_RUNTIME_ASSERT(_delayedScheduler);
    const auto now = GetGameEngine(_asEngine.get())->GameTime.GetFrameTime();
    const auto delay = time > now ? time - now : timespan::zero;
    _delayedScheduler(delay, [this, ctx]() { ResumeSpecificContext(ctx); });
}

void AngelScriptContextManager::ResumeSpecificContext(AngelScript::asIScriptContext* ctx)
{
    FO_STACK_TRACE_ENTRY();

    {
        scoped_lock lock {_poolLocker};

        const auto it = std::ranges::find_if(_busyContexts, [ctx](const auto& busy) { return busy.get() == ctx; });
        FO_RUNTIME_ASSERT(it != _busyContexts.end());

        if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
            return;
        }
    }

    try {
        auto return_ctx = scope_exit([&]() noexcept { ReturnContext(ctx); });
        RunContext(ctx, true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

static auto BuildScriptFrameForContext(AngelScript::asIScriptContext* ctx, uint32_t stack_level) noexcept -> optional<StackTraceFrame>
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto as_stack_level = numeric_cast<AngelScript::asUINT>(stack_level);
        const auto* func = ctx->GetFunction(as_stack_level);

        if (func == nullptr) {
            return std::nullopt;
        }

        const int32_t ctx_line = ctx->GetLineNumber(as_stack_level);
        const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));

        StackTraceFrame frame;
        frame.Type = StackTraceFrame::FrameType::Script;

        if (const auto* decl = func->GetDeclaration(true); decl != nullptr) {
            frame.Function = decl;
        }
        else if (const auto* name = func->GetName(); name != nullptr) {
            frame.Function = name;
        }

        if (ctx_line != 0 && lnt != nullptr) {
            frame.File = Preprocessor::ResolveOriginalFile(ctx_line, lnt);
            frame.Line = numeric_cast<uint32_t>(Preprocessor::ResolveOriginalLine(ctx_line, lnt));
        }
        else {
            frame.Line = ctx_line > 0 ? numeric_cast<uint32_t>(ctx_line) : 0u;
        }

        return frame;
    }
    catch (...) {
        return std::nullopt;
    }
}

static void CollectScriptStackLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        auto* ctx = AngelScript::asGetActiveContext();

        while (ctx != nullptr) {
            auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);

            ScriptStackTraceLayer layer;
            const auto callstack_size = ctx->GetCallstackSize();
            layer.ScriptFrames.reserve(callstack_size);

            for (AngelScript::asUINT i = 0; i < callstack_size; i++) {
                if (auto frame = BuildScriptFrameForContext(ctx, numeric_cast<uint32_t>(i)); frame.has_value()) {
                    layer.ScriptFrames.emplace_back(std::move(*frame));
                }
            }

            if (ctx_ext != nullptr) {
                layer.BirthNativeFrames = ctx_ext->BirthNativeFrames;
                layer.BirthNativeFrameCount = ctx_ext->BirthNativeFrameCount;
                layer.BirthNativeTruncated = ctx_ext->BirthNativeTruncated;
            }

            if (!layer.ScriptFrames.empty() || layer.BirthNativeFrameCount != 0) {
                out_layers.emplace_back(std::move(layer));
            }

            ctx = ctx_ext != nullptr ? ctx_ext->Parent.get() : nullptr;
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void AngelScriptTranslateAppException(AngelScript::asIScriptContext* ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(param);

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_RUNTIME_ASSERT(ctx_ext);
    ctx_ext->Exception = std::current_exception();
}

static void AngelScriptException(AngelScript::asIScriptContext* ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(param);

    // Count exceptions
    auto* backend = GetScriptBackend(ctx->GetEngine());
    backend->IncreaseExceptionCounter();

    for (auto* ctx_iter = ctx; ctx_iter != nullptr;) {
        auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx_iter);
        FO_RUNTIME_ASSERT(ctx_ext);
        ctx_ext->ExceptionCount++;
        ctx_iter = ctx_ext->Parent.get();
    }

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_RUNTIME_ASSERT(ctx_ext);
    auto& ex = ctx_ext->Exception;

    if (ex) {
        return;
    }

    // Capture the script call stack as it stands right now.
    ex = std::make_exception_ptr(ScriptCoreException(ctx->GetExceptionString()));
}

#if FO_TRACY
static void AngelScriptBeginCall(AngelScript::asIScriptContext* ctx, AngelScript::asIScriptFunction* func)
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (ctx_ext == nullptr || !ctx_ext->TracyExecutionActive) {
        return;
    }

    AngelScriptTracyCallEntry* entry = nullptr;
    const auto func_key = std::bit_cast<size_t>(func);

    {
        scoped_lock lock {AngelScriptTracy->CacheLocker};

        if (const auto it = AngelScriptTracy->CacheEntries.find(func_key); it != AngelScriptTracy->CacheEntries.end()) {
            entry = &it->second;
        }
    }

    if (entry == nullptr) {
        const int32_t ctx_line = ctx->GetLineNumber();
        const auto* lnt = cast_from_void<Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
        const auto& orig_file = Preprocessor::ResolveOriginalFile(ctx_line, lnt);
        const auto orig_line = numeric_cast<uint32_t>(Preprocessor::ResolveOriginalLine(ctx_line, lnt));
        const auto* func_decl = func->GetDeclaration(true);

        scoped_lock lock {AngelScriptTracy->CacheLocker};

        entry = &AngelScriptTracy->CacheEntries.emplace(func_key, AngelScriptTracyCallEntry {}).first->second;

        const auto safe_copy = [](auto& to, size_t& len, string_view from) {
            len = std::min(from.length(), to.size() - 1);
            MemCopy(to.data(), from.data(), len);
            to[len] = 0;
        };

        safe_copy(entry->FuncBuf, entry->FuncBufLen, func_decl != nullptr ? string_view {func_decl} : string_view {});
        safe_copy(entry->FileBuf, entry->FileBufLen, orig_file);
        entry->Line = orig_line;
    }

    ctx_ext->TracyExecutionCalls++;
    ctx_ext->TracyStackTrace.emplace_back(entry);

    const auto tracy_srcloc = ___tracy_alloc_srcloc(entry->Line, entry->FileBuf.data(), entry->FileBufLen, entry->FuncBuf.data(), entry->FuncBufLen, 0);
    const auto tracy_zone = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
    ctx_ext->TracyZones.emplace_back(tracy_zone);
}

static void AngelScriptEndCall(AngelScript::asIScriptContext* ctx) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto* ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (ctx_ext == nullptr || !ctx_ext->TracyExecutionActive) {
        return;
    }

    FO_STRONG_ASSERT(ctx_ext->TracyExecutionCalls > 0);

    if (ctx_ext->TracyExecutionCalls > 0) {
        ctx_ext->TracyExecutionCalls--;
        ctx_ext->TracyStackTrace.pop_back();
        ___tracy_emit_zone_end(ctx_ext->TracyZones.back());
        ctx_ext->TracyZones.pop_back();
    }
}
#endif

FO_END_NAMESPACE

#endif
