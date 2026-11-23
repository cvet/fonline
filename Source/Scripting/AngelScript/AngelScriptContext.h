// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "EngineBase.h"
#include "Entity.h"

namespace AngelScript
{
    class asIScriptContext;
    class asIScriptEngine;
    class asIScriptFunction;
}

FO_BEGIN_NAMESPACE

struct DebuggerStepState;
class Property;

#if FO_TRACE_CATEGORY_ENABLED(Script)
struct AngelScriptTracyCallEntry;
#endif

enum class AngelScriptContextSetupReason : uint8_t
{
    Create,
    Request,
    Return,
};

struct AngelScriptContextExtendedData
{
    nptr<AngelScript::asIScriptContext> Root {};
    nptr<AngelScript::asIScriptContext> Parent {};
    int32_t ExceptionCount {};
    shared_ptr<DebuggerStepState> StepState {};
    std::exception_ptr Exception {};
    function<void(bool)> Completion {};
    std::array<stack_trace::native_frame_address, stack_trace::MAX_NATIVE_FRAMES> BirthNativeFrames {};
    uint32_t BirthNativeFrameCount {};
    bool BirthNativeTruncated {};
    std::atomic_bool ExecutionActive {};
    std::atomic_bool ExecutionSuspended {};
    std::atomic<uint64_t> Generation {};

#if FO_TRACE_CATEGORY_ENABLED(Script)
    bool TracyExecutionActive {};
    size_t TracyExecutionCalls {};
    vector<nptr<const AngelScriptTracyCallEntry>> TracyStackTrace {};
    vector<TracyCZoneCtx> TracyZones {};
#endif
};

auto GetAngelScriptContextData(ptr<AngelScript::asIScriptContext> ctx) -> nptr<AngelScriptContextExtendedData>;
auto GetAngelScriptContextData(ptr<const AngelScript::asIScriptContext> ctx) -> nptr<const AngelScriptContextExtendedData>;
auto GetAngelScriptContextData(nptr<AngelScript::asIScriptContext> ctx) -> nptr<AngelScriptContextExtendedData>;
auto GetAngelScriptContextData(nptr<const AngelScript::asIScriptContext> ctx) -> nptr<const AngelScriptContextExtendedData>;

class AngelScriptContextManager final
{
public:
    using DelayedScheduler = function<void(timespan delay, function<void()> body)>;

    struct Diagnostics
    {
        size_t FreeContexts {};
        size_t BusyContexts {};
        size_t SuspendedContexts {};
        size_t ActiveContexts {};
        size_t OtherBusyContexts {};
    };

    explicit AngelScriptContextManager(ptr<AngelScript::asIScriptEngine> as_engine, ptr<BaseEngine> engine, timespan overrun_timeout, function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> debugger_stop_callback = nullptr);
    AngelScriptContextManager(const AngelScriptContextManager&) noexcept = delete;
    auto operator=(const AngelScriptContextManager&) noexcept -> AngelScriptContextManager& = delete;
    AngelScriptContextManager(AngelScriptContextManager&&) noexcept = delete;
    auto operator=(AngelScriptContextManager&&) noexcept -> AngelScriptContextManager& = delete;
    ~AngelScriptContextManager() FO_TSA_NO_ANALYSIS; // Single-threaded teardown drains both context pools without the lock

    [[nodiscard]] auto GetDiagnostics() const -> Diagnostics;

    auto RequestContext() -> ptr<AngelScript::asIScriptContext>;
    void ReturnContext(ptr<AngelScript::asIScriptContext> ctx, uint64_t expected_generation) noexcept;
    auto GetContextGeneration(ptr<AngelScript::asIScriptContext> ctx) const noexcept -> uint64_t;
    auto PrepareContext(ptr<AngelScript::asIScriptFunction> func) -> ptr<AngelScript::asIScriptContext>;
    void SetContextSetupCallback(function<void(ptr<AngelScript::asIScriptContext>, AngelScriptContextSetupReason)> context_setup_callback);
    auto RunContext(ptr<AngelScript::asIScriptContext> ctx, bool can_suspend, bool execution_reserved = false) -> bool;
    void SuspendScriptContext(ptr<AngelScript::asIScriptContext> ctx, nanotime time);
    void ResumeSpecificContext(ptr<AngelScript::asIScriptContext> ctx);

    void SetDelayedScheduler(DelayedScheduler scheduler) { _delayedScheduler = std::move(scheduler); }

private:
    void CreateContext() FO_TSA_REQUIRES(_poolLocker);

    ptr<AngelScript::asIScriptEngine> _asEngine;
    ptr<BaseEngine> _engine;
    timespan _overrunTimeout;
    mutable mutex _poolLocker {};
    vector<refcount_ptr<AngelScript::asIScriptContext>> _freeContexts FO_TSA_GUARDED_BY(_poolLocker) {};
    vector<refcount_ptr<AngelScript::asIScriptContext>> _busyContexts FO_TSA_GUARDED_BY(_poolLocker) {};
    function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> _debuggerStopCallback {};
    function<void(ptr<AngelScript::asIScriptContext>, AngelScriptContextSetupReason)> _contextSetupCallback {};
    DelayedScheduler _delayedScheduler {};
};

FO_END_NAMESPACE

#endif
