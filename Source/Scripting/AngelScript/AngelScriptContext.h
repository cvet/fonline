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

#if FO_TRACY
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
    std::array<NativeStackFrameAddress, STACK_TRACE_MAX_NATIVE_FRAMES> BirthNativeFrames {};
    uint32_t BirthNativeFrameCount {};
    bool BirthNativeTruncated {};
    std::atomic_bool ExecutionActive {};
    std::atomic<uint64_t> Generation {};

#if FO_TRACY
    bool TracyExecutionActive {};
    size_t TracyExecutionCalls {};
    vector<nptr<const AngelScriptTracyCallEntry>> TracyStackTrace {};
    vector<TracyCZoneCtx> TracyZones {};
#endif

    static auto Get(ptr<AngelScript::asIScriptContext> ctx) -> nptr<AngelScriptContextExtendedData>;
    static auto Get(ptr<const AngelScript::asIScriptContext> ctx) -> nptr<const AngelScriptContextExtendedData>;
};

class AngelScriptContextManager final
{
public:
    using DelayedScheduler = function<void(timespan delay, function<void()> body)>;

    explicit AngelScriptContextManager(ptr<AngelScript::asIScriptEngine> as_engine, timespan overrun_timeout, function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> debugger_stop_callback = nullptr);
    AngelScriptContextManager(const AngelScriptContextManager&) noexcept = delete;
    auto operator=(const AngelScriptContextManager&) noexcept -> AngelScriptContextManager& = delete;
    AngelScriptContextManager(AngelScriptContextManager&&) noexcept = delete;
    auto operator=(AngelScriptContextManager&&) noexcept -> AngelScriptContextManager& = delete;
    ~AngelScriptContextManager() FO_TSA_NO_ANALYSIS; // Single-threaded teardown drains both context pools without the lock

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
    timespan _overrunTimeout;
    mutable mutex _poolLocker {};
    vector<refcount_ptr<AngelScript::asIScriptContext>> _freeContexts FO_TSA_GUARDED_BY(_poolLocker) {};
    vector<refcount_ptr<AngelScript::asIScriptContext>> _busyContexts FO_TSA_GUARDED_BY(_poolLocker) {};
    bool _nonConstHelper {};
    function<void(string_view, string_view, string_view, optional<uint32_t>, string_view)> _debuggerStopCallback {};
    function<void(ptr<AngelScript::asIScriptContext>, AngelScriptContextSetupReason)> _contextSetupCallback {};
    DelayedScheduler _delayedScheduler {};
};

FO_END_NAMESPACE

#endif
