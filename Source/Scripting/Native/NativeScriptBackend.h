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

#if FO_NATIVE_SCRIPTING

#include "EngineBase.h"
#include "NativeScriptCore.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

class NativeScriptBackend final : public ScriptSystemBackend
{
public:
    explicit NativeScriptBackend(const ScriptSettings& settings);
    NativeScriptBackend(const NativeScriptBackend&) = delete;
    NativeScriptBackend(NativeScriptBackend&&) noexcept = delete;
    auto operator=(const NativeScriptBackend&) -> NativeScriptBackend& = delete;
    auto operator=(NativeScriptBackend&&) noexcept -> NativeScriptBackend& = delete;
    ~NativeScriptBackend() override;

    void Attach(ptr<EngineMetadata> meta);

    // Build a target-neutral module context from the attached engine for handing off to
    // user-side module init callbacks. The actual call site lives in the
    // per-target startup glue (Server.cpp / Client.cpp / Mapper.cpp) which
    // invokes the role-specific `RegisterNativeScriptModules_<Role>(ctx)`
    // dispatcher emitted by LF_NativeScriptSynth. `isBaker` marks the
    // MasterBaker-owned context, whose BaseEngine pointer is intentionally null.
    [[nodiscard]] auto MakeContext(bool isBaker = false) noexcept -> NativeScripts::ModuleInitContextBase;

    [[nodiscard]] auto GetEngine() noexcept -> nptr<BaseEngine> { return _engine; }
    [[nodiscard]] auto GetScriptSys() noexcept -> nptr<ScriptSystem> { return _scriptSys; }
    [[nodiscard]] auto GetMetadata() noexcept -> nptr<EngineMetadata> { return _meta; }

    // Storage for `ScriptFuncDesc` instances minted by
    // `NativeScripts::MakeScriptFunc`. `ScriptFunc<...>` references the desc
    // by raw pointer with a no-op deleter, so the desc must outlive every
    // consumer (TimeEventMngr slots, dynamic-event subscriptions persisting
    // a ScriptFunc copy, ...). `deque` keeps element addresses stable across
    // appends; the container drains together with the backend at engine
    // shutdown — the same lifetime AS function descs get from the AS engine.
    [[nodiscard]] auto AllocateScriptFunc() -> ScriptFuncDesc& { return _scriptFuncs.emplace_back(); }

private:
    ptr<const ScriptSettings> _settings;
    nptr<EngineMetadata> _meta {};
    nptr<ScriptSystem> _scriptSys {};
    nptr<BaseEngine> _engine {};
    deque<ScriptFuncDesc> _scriptFuncs {};
};

FO_END_NAMESPACE

#endif
