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

#include <angelscript.h>

#if FO_ANGELSCRIPT_SCRIPTING

FO_BEGIN_NAMESPACE

class AngelScriptBackend;
enum class AngelScriptContextSetupReason : uint8;

enum class DebuggerStepMode : uint8
{
    None,
    In,
    Over,
    Out,
};

class DebuggerEndpointServer final
{
public:
    explicit DebuggerEndpointServer(const AngelScriptBackend* backend);
    DebuggerEndpointServer(const DebuggerEndpointServer&) = delete;
    auto operator=(const DebuggerEndpointServer&) = delete;
    DebuggerEndpointServer(DebuggerEndpointServer&&) noexcept = delete;
    auto operator=(DebuggerEndpointServer&&) noexcept = delete;
    ~DebuggerEndpointServer();

    [[nodiscard]] auto IsPaused() const noexcept -> bool;

    void SetupContext(AngelScript::asIScriptContext* ctx, AngelScriptContextSetupReason reason);
    void EmitEvent(string_view event_name, string_view body_json = "{}");
    void Stop();

private:
    static void AngelScriptLine(AngelScript::asIScriptContext* ctx, void* param);

    class Impl;
    unique_ptr<Impl> _impl {};
};

FO_END_NAMESPACE

#endif
