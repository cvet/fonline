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

#include "WinApiUndef.inc" // Remove garbage from includes above

FO_BEGIN_NAMESPACE

static auto BuildScriptFrameForContext(ptr<AngelScript::asIScriptContext> ctx, uint32_t stack_level) noexcept -> optional<StackTraceFrame>;
static void CollectScriptStackLayers(std::vector<ScriptStackTraceLayer>& out_layers) noexcept;

static auto IsSameScriptContext(ptr<const AngelScript::asIScriptContext> lhs, ptr<const AngelScript::asIScriptContext> rhs) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return lhs == rhs;
}

struct AngelScriptStackTraceInstaller
{
    AngelScriptStackTraceInstaller() noexcept { SetScriptStackTraceProvider(&CollectScriptStackLayers); }
};
FO_GLOBAL_DATA(AngelScriptStackTraceInstaller, AngelScriptStackTraceInstall);

static void AngelScriptTranslateAppException(AngelScript::asIScriptContext* raw_ctx, void* param);
static void AngelScriptException(AngelScript::asIScriptContext* raw_ctx, void* param);

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

static void AngelScriptBeginCall(AngelScript::asIScriptContext* raw_ctx, AngelScript::asIScriptFunction* raw_func);
static void AngelScriptEndCall(AngelScript::asIScriptContext* raw_ctx) noexcept;
#endif

static void CleanupScriptContextExtendedData(ptr<AngelScriptContextExtendedData> ctx_ext) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    auto owned_ctx_ext = adopt_unique_ptr(ctx_ext);
    ignore_unused(owned_ctx_ext);
}

static void CleanupScriptContext(AngelScript::asIScriptContext* raw_ctx)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_ctx != nullptr, "Missing script execution context");
    auto ctx = make_ptr(raw_ctx);
    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (ctx_ext) {
        CleanupScriptContextExtendedData(ctx_ext);
    }
}

auto AngelScriptContextExtendedData::Get(ptr<AngelScript::asIScriptContext> ctx) -> nptr<AngelScriptContextExtendedData>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<AngelScriptContextExtendedData*>(ctx->GetUserData());
}

auto AngelScriptContextExtendedData::Get(ptr<const AngelScript::asIScriptContext> ctx) -> nptr<const AngelScriptContextExtendedData>
{
    FO_NO_STACK_TRACE_ENTRY();

    return cast_from_void<const AngelScriptContextExtendedData*>(ctx->GetUserData());
}

AngelScriptContextManager::AngelScriptContextManager(ptr<AngelScript::asIScriptEngine> as_engine, timespan overrun_timeout, function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> debugger_stop_callback) :
    _asEngine {as_engine},
    _overrunTimeout {overrun_timeout},
    _debuggerStopCallback {std::move(debugger_stop_callback)}
{
    FO_STACK_TRACE_ENTRY();

    _asEngine->SetContextUserDataCleanupCallback(CleanupScriptContext);

    int32_t as_result = 0;
    FO_AS_VERIFY(_asEngine->SetTranslateAppExceptionCallback(asFUNCTION(AngelScriptTranslateAppException), nullptr, AngelScript::asCALL_CDECL));
}

AngelScriptContextManager::~AngelScriptContextManager()
{
    FO_STACK_TRACE_ENTRY();

    const auto cleanup_context = [](ptr<AngelScript::asIScriptContext> ctx) {
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

    for (size_t i = 0; i < _busyContexts.size(); i++) {
        cleanup_context(_busyContexts[i]);
    }
    for (size_t i = 0; i < _freeContexts.size(); i++) {
        cleanup_context(_freeContexts[i]);
    }
}

void AngelScriptContextManager::CreateContext()
{
    FO_STACK_TRACE_ENTRY();

    nptr<AngelScript::asIScriptContext> ctx = _asEngine->CreateContext();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    _freeContexts.emplace_back(refcount_ptr<AngelScript::asIScriptContext>::from_adopted_ref(ctx.get()));
    auto ctx_ptr = _freeContexts.back().as_ptr();

    auto ctx_ext = SafeAlloc::MakeUnique<AngelScriptContextExtendedData>();
#if FO_TRACY
    ctx_ext->TracyStackTrace.reserve(128);
    ctx_ext->TracyZones.reserve(128);
#endif
    auto released_ctx_ext = ctx_ext.release();
    ctx_ptr->SetUserData(released_ctx_ext.void_cast());

#if FO_TRACY
    auto ctx_impl = ctx_ptr.dyn_cast<AngelScript::asCContext>();
    FO_VERIFY_AND_THROW(ctx_impl, "Script context implementation cast failed");
    ctx_impl->BeginScriptCall = AngelScriptBeginCall;
    ctx_impl->EndScriptCall = AngelScriptEndCall;
#endif

    int32_t as_result = 0;
    FO_AS_VERIFY(ctx_ptr->SetExceptionCallback(asFUNCTION(AngelScriptException), nullptr, AngelScript::asCALL_CDECL));

    if (_contextSetupCallback) {
        _contextSetupCallback(ctx_ptr, AngelScriptContextSetupReason::Create);
    }
}

static void EnsureAngelScriptThreadStorageReleasedOnExit() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    struct ThreadStorageGuard
    {
        ~ThreadStorageGuard() noexcept { AngelScript::asThreadCleanup(); }
    };

    static thread_local ThreadStorageGuard guard;
    ignore_unused(guard);
}

auto AngelScriptContextManager::RequestContext() -> ptr<AngelScript::asIScriptContext>
{
    FO_STACK_TRACE_ENTRY();

    EnsureAngelScriptThreadStorageReleasedOnExit();

    scoped_lock lock {_poolLocker};

    if (_freeContexts.empty()) {
        CreateContext();
    }

    auto ctx = _freeContexts.back();
    auto ctx_ext = AngelScriptContextExtendedData::Get(ptr<AngelScript::asIScriptContext> {ctx});
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");

    _freeContexts.pop_back();
    vec_add_unique_value(_busyContexts, ctx);

    auto restore_to_free = scope_fail([&]() FO_TSA_NO_ANALYSIS noexcept {
        vec_remove_unique_value(_busyContexts, ctx);
        vec_add_unique_value(_freeContexts, ctx);
    });

    ctx_ext->Generation++;

    auto parent_ctx = make_nptr(AngelScript::asGetActiveContext());
    nptr<AngelScriptContextExtendedData> parent_ctx_ext = parent_ctx ? AngelScriptContextExtendedData::Get(parent_ctx.as_ptr()) : nullptr;
    nptr<AngelScript::asIScriptContext> root_ctx = parent_ctx;

    if (parent_ctx_ext) {
        root_ctx = parent_ctx_ext->Root;
    }

    ctx_ext->Parent = parent_ctx;
    ctx_ext->Root = root_ctx;
    if (!ctx_ext->Root) {
        ctx_ext->Root = ctx.get();
    }
    ctx_ext->Exception = {};

    CaptureNativeStackFrames(ctx_ext->BirthNativeFrames, ctx_ext->BirthNativeFrameCount, ctx_ext->BirthNativeTruncated, 1);

    if (_contextSetupCallback) {
        _contextSetupCallback(ctx, AngelScriptContextSetupReason::Request);
    }

    return ctx;
}

void AngelScriptContextManager::ReturnContext(ptr<AngelScript::asIScriptContext> ctx, uint64_t expected_generation) noexcept
{
    FO_STACK_TRACE_ENTRY();

    try {
        scoped_lock lock {_poolLocker};

        const auto it = std::ranges::find_if(_busyContexts, [ctx](const auto& busy) { return IsSameScriptContext(busy, ctx); });

        if (it == _busyContexts.end()) {
            return;
        }

        auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
        FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");

        if (ctx_ext->Generation.load() != expected_generation) {
            return;
        }

        if (ctx_ext->ExecutionActive.load()) {
            return;
        }

        const AngelScript::asEContextState state = ctx->GetState();

        if (state == AngelScript::asEXECUTION_SUSPENDED || state == AngelScript::asEXECUTION_ACTIVE) {
            return;
        }

        int32_t as_result = 0;
        FO_AS_VERIFY(ctx->Unprepare());

        auto ctx_holder = refcount_ptr<AngelScript::asIScriptContext>::from_add_ref(ctx.get());

        if (_contextSetupCallback) {
            _contextSetupCallback(ctx_holder, AngelScriptContextSetupReason::Return);
        }

        ctx_ext->Parent = nullptr;
        ctx_ext->Root = nullptr;
        ctx_ext->Exception = {};
        ctx_ext->BirthNativeFrameCount = 0;
        ctx_ext->BirthNativeTruncated = false;

        for (auto& other : _busyContexts) {
            if (IsSameScriptContext(other, ctx)) {
                continue;
            }

            auto other_ext = AngelScriptContextExtendedData::Get(ptr<AngelScript::asIScriptContext> {other});
            FO_VERIFY_AND_THROW(other_ext, "Context extended data is null");

            if (other_ext->Parent == ctx.get()) {
                other_ext->Parent.reset();
            }
            if (other_ext->Root == ctx.get()) {
                other_ext->Root.reset();
            }
        }

        // Single atomic busy->free move; reserve first so the add cannot half-complete.
        vec_remove_unique_value(_busyContexts, ctx_holder);
        vec_add_unique_value(_freeContexts, ctx_holder);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

auto AngelScriptContextManager::PrepareContext(ptr<AngelScript::asIScriptFunction> func) -> ptr<AngelScript::asIScriptContext>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = RequestContext();
    const auto ctx_generation = GetContextGeneration(ctx);
    const auto as_result = ctx->Prepare(func.get());

    if (as_result < 0) {
        ReturnContext(ctx, ctx_generation);
        throw ScriptCallException("Can't prepare context", func->GetName(), as_result);
    }

    return ctx;
}

auto AngelScriptContextManager::GetContextGeneration(ptr<AngelScript::asIScriptContext> ctx) const noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (!ctx_ext) {
        return 0;
    }

    return ctx_ext->Generation.load();
}

void AngelScriptContextManager::SetContextSetupCallback(function<void(ptr<AngelScript::asIScriptContext>, AngelScriptContextSetupReason)> context_setup_callback)
{
    FO_STACK_TRACE_ENTRY();

    _contextSetupCallback = std::move(context_setup_callback);
}

auto AngelScriptContextManager::RunContext(ptr<AngelScript::asIScriptContext> ctx, bool can_suspend, bool execution_reserved) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");

    if (execution_reserved) {
        FO_VERIFY_AND_THROW(ctx_ext->ExecutionActive.load(), "Script execution context is not reserved");
    }
    else {
        const bool already_active = ctx_ext->ExecutionActive.exchange(true);
        FO_VERIFY_AND_THROW(!already_active, "Already active is already set");
    }

    auto execution_active_guard = scope_exit([ctx_ext]() mutable noexcept { ctx_ext->ExecutionActive.store(false); });

    int32_t exec_result = 0;

    {
#if FO_TRACY
        FO_VERIFY_AND_THROW(!ctx_ext->TracyExecutionActive, "Tracy script execution scope is already active");
        FO_VERIFY_AND_THROW(!ctx_ext->TracyExecutionCalls, "Tracy script execution call depth is not zero");
        ctx_ext->TracyExecutionActive = true;

        if (ctx->GetState() == AngelScript::asEXECUTION_SUSPENDED) {
            for (nptr<const AngelScriptTracyCallEntry> entry : ctx_ext->TracyStackTrace) {
                FO_VERIFY_AND_THROW(entry, "Missing Tracy call stack entry");

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
            auto exc_ctx_ext = AngelScriptContextExtendedData::Get(ctx);
            FO_VERIFY_AND_THROW(exc_ctx_ext, "Missing extended script execution context");
            const auto ex = exc_ctx_ext->Exception;

            if (_debuggerStopCallback) {
                string source_path;
                optional<uint32_t> source_line;
                string function_name;

                if (auto ex_func = ctx->GetExceptionFunction(); ex_func) {
                    function_name = ex_func->GetName();
                }

                if (const auto ex_line = ctx->GetExceptionLineNumber(); ex_line != 0) {
                    auto lnt = cast_from_void<const Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));
                    const string_view ex_orig_file = Preprocessor::ResolveOriginalFile(ex_line, lnt.get());
                    const auto ex_orig_line = Preprocessor::ResolveOriginalLine(ex_line, lnt.get());

                    source_path = string {ex_orig_file};

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

void AngelScriptContextManager::SuspendScriptContext(ptr<AngelScript::asIScriptContext> ctx, nanotime time)
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
        ctx->Suspend();
    }

    FO_VERIFY_AND_THROW(_delayedScheduler, "Missing required delayed scheduler");
    const auto now = GetGameEngine(_asEngine)->GameTime.GetFrameTime();
    const auto delay = time > now ? time - now : timespan::zero;
    _delayedScheduler(delay, [this, ctx]() { ResumeSpecificContext(ctx); });
}

void AngelScriptContextManager::ResumeSpecificContext(ptr<AngelScript::asIScriptContext> ctx)
{
    FO_STACK_TRACE_ENTRY();

    uint64_t ctx_generation = 0;

    {
        scoped_lock lock {_poolLocker};

        const auto it = std::ranges::find_if(_busyContexts, [ctx](const auto& busy) { return IsSameScriptContext(busy, ctx); });

        if (it == _busyContexts.end()) {
            return;
        }

        auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
        FO_STRONG_ASSERT(ctx_ext, "Missing extended script execution context");

        if (ctx_ext->ExecutionActive.exchange(true)) {
            FO_STRONG_ASSERT(_delayedScheduler, "Missing required delayed scheduler");
            _delayedScheduler(std::chrono::milliseconds(1), [this, ctx]() { ResumeSpecificContext(ctx); });
            return;
        }

        if (ctx->GetState() != AngelScript::asEXECUTION_SUSPENDED) {
            ctx_ext->ExecutionActive.store(false);
            return;
        }

        ctx_generation = ctx_ext->Generation.load();
    }

    try {
        auto return_ctx = scope_exit([&]() noexcept { ReturnContext(ctx, ctx_generation); });
        RunContext(ctx, true, true);
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
}

static auto BuildScriptFrameForContext(ptr<AngelScript::asIScriptContext> ctx, uint32_t stack_level) noexcept -> optional<StackTraceFrame>
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        const auto as_stack_level = numeric_cast<AngelScript::asUINT>(stack_level);
        nptr<const AngelScript::asIScriptFunction> func = ctx->GetFunction(as_stack_level);

        if (!func) {
            return std::nullopt;
        }

        const int32_t ctx_line = ctx->GetLineNumber(as_stack_level);
        auto lnt = cast_from_void<const Preprocessor::LineNumberTranslator*>(ctx->GetEngine()->GetUserData(5));

        StackTraceFrame frame;
        frame.Type = StackTraceFrame::FrameType::Script;

        if (auto decl = make_nptr(func->GetDeclaration(true)); decl) {
            frame.Function = decl.get();
        }
        else if (auto name = make_nptr(func->GetName()); name) {
            frame.Function = name.get();
        }

        if (ctx_line != 0 && lnt) {
            frame.File = Preprocessor::ResolveOriginalFile(ctx_line, lnt.get());
            frame.Line = numeric_cast<uint32_t>(Preprocessor::ResolveOriginalLine(ctx_line, lnt.get()));
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
        // asGetActiveContext below lazily allocates AngelScript thread-local storage on first access, including
        // on threads that only capture a stack trace (never run a script). Register the per-thread cleanup so
        // that storage is released when the thread exits rather than leaked.
        EnsureAngelScriptThreadStorageReleasedOnExit();

        auto ctx = make_nptr(AngelScript::asGetActiveContext());

        while (ctx) {
            auto ctx_ext = AngelScriptContextExtendedData::Get(ctx.as_ptr());

            ScriptStackTraceLayer layer;
            const auto callstack_size = ctx->GetCallstackSize();
            layer.ScriptFrames.reserve(callstack_size);

            for (AngelScript::asUINT i = 0; i < callstack_size; i++) {
                if (auto frame = BuildScriptFrameForContext(ctx, numeric_cast<uint32_t>(i)); frame.has_value()) {
                    layer.ScriptFrames.emplace_back(std::move(*frame));
                }
            }

            if (ctx_ext) {
                layer.BirthNativeFrames = ctx_ext->BirthNativeFrames;
                layer.BirthNativeFrameCount = ctx_ext->BirthNativeFrameCount;
                layer.BirthNativeTruncated = ctx_ext->BirthNativeTruncated;
            }

            if (!layer.ScriptFrames.empty() || layer.BirthNativeFrameCount != 0) {
                out_layers.emplace_back(std::move(layer));
            }

            if (ctx_ext) {
                ctx = ctx_ext->Parent;
            }
            else {
                ctx = nullptr;
            }
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

static void AngelScriptTranslateAppException(AngelScript::asIScriptContext* raw_ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(param);

    FO_VERIFY_AND_THROW(raw_ctx != nullptr, "Missing script execution context");
    auto ctx = make_ptr(raw_ctx);
    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");
    ctx_ext->Exception = std::current_exception();
}

static void AngelScriptException(AngelScript::asIScriptContext* raw_ctx, void* param)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(param);

    FO_VERIFY_AND_THROW(raw_ctx != nullptr, "Missing script execution context");
    auto ctx = make_ptr(raw_ctx);

    // Count exceptions
    ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
    auto backend = GetScriptBackend(as_engine);
    backend->IncreaseExceptionCounter();

    for (auto ctx_iter = ctx.as_nptr(); ctx_iter;) {
        auto ctx_ext = AngelScriptContextExtendedData::Get(ctx_iter.as_ptr());
        FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");
        ctx_ext->ExceptionCount++;
        ctx_iter = ctx_ext->Parent;
    }

    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);
    FO_VERIFY_AND_THROW(ctx_ext, "Missing extended script execution context");
    std::exception_ptr& ex = ctx_ext->Exception;

    if (ex) {
        return;
    }

    // Capture the script call stack as it stands right now.
    ex = std::make_exception_ptr(ScriptCoreException(ctx->GetExceptionString()));
}

#if FO_TRACY
static void AngelScriptBeginCall(AngelScript::asIScriptContext* raw_ctx, AngelScript::asIScriptFunction* raw_func)
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(raw_ctx != nullptr, "Missing script execution context");
    auto ctx = make_ptr(raw_ctx);
    FO_VERIFY_AND_THROW(raw_func != nullptr, "Missing called script function");
    auto func = make_ptr(raw_func);
    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (!ctx_ext || !ctx_ext->TracyExecutionActive) {
        return;
    }

    nptr<AngelScriptTracyCallEntry> entry {};
    const auto func_key = std::bit_cast<size_t>(func.get());

    {
        scoped_lock lock {AngelScriptTracy->CacheLocker};

        if (const auto it = AngelScriptTracy->CacheEntries.find(func_key); it != AngelScriptTracy->CacheEntries.end()) {
            entry = &it->second;
        }
    }

    if (!entry) {
        const int32_t ctx_line = ctx->GetLineNumber();
        ptr<AngelScript::asIScriptEngine> as_engine = ctx->GetEngine();
        auto lnt = cast_from_void<const Preprocessor::LineNumberTranslator*>(as_engine->GetUserData(5));
        const string_view orig_file = Preprocessor::ResolveOriginalFile(ctx_line, lnt.get());
        const auto orig_line = numeric_cast<uint32_t>(Preprocessor::ResolveOriginalLine(ctx_line, lnt.get()));
        const nptr<const char> func_decl = func->GetDeclaration(true);

        scoped_lock lock {AngelScriptTracy->CacheLocker};

        entry = &AngelScriptTracy->CacheEntries.emplace(func_key, AngelScriptTracyCallEntry {}).first->second;

        const auto safe_copy = [](auto& to, size_t& len, string_view from) {
            len = std::min(from.length(), to.size() - 1);
            MemCopy(to.data(), from.data(), len);
            to[len] = 0;
        };

        safe_copy(entry->FuncBuf, entry->FuncBufLen, func_decl ? string_view {func_decl.get()} : string_view {});
        safe_copy(entry->FileBuf, entry->FileBufLen, orig_file);
        entry->Line = orig_line;
    }

    ctx_ext->TracyExecutionCalls++;
    ctx_ext->TracyStackTrace.emplace_back(entry);

    const auto tracy_srcloc = ___tracy_alloc_srcloc(entry->Line, entry->FileBuf.data(), entry->FileBufLen, entry->FuncBuf.data(), entry->FuncBufLen, 0);
    const auto tracy_zone = ___tracy_emit_zone_begin_alloc(tracy_srcloc, 1);
    ctx_ext->TracyZones.emplace_back(tracy_zone);
}

static void AngelScriptEndCall(AngelScript::asIScriptContext* raw_ctx) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (raw_ctx == nullptr) {
        return;
    }

    auto ctx = make_ptr(raw_ctx);
    auto ctx_ext = AngelScriptContextExtendedData::Get(ctx);

    if (!ctx_ext || !ctx_ext->TracyExecutionActive) {
        return;
    }

    FO_STRONG_ASSERT(ctx_ext->TracyExecutionCalls > 0, "AngelScript Tracy call stack underflow", ctx_ext->TracyExecutionCalls);

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
