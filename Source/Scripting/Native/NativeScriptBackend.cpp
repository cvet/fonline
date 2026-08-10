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

#include "NativeScriptBackend.h"

#if FO_NATIVE_SCRIPTING

#include "NativeScriptCore.h"

FO_BEGIN_NAMESPACE

NativeScriptBackend::NativeScriptBackend(const ScriptSettings& settings) :
    _settings {&settings}
{
    FO_STACK_TRACE_ENTRY();
}

NativeScriptBackend::~NativeScriptBackend()
{
    FO_NO_STACK_TRACE_ENTRY();
}

void NativeScriptBackend::Attach(EngineMetadata* meta)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(meta);

    _meta = meta;
    _engine = dynamic_cast<BaseEngine*>(meta);
    _scriptSys = dynamic_cast<ScriptSystem*>(meta);
}

auto NativeScriptBackend::MakeContext(bool isBaker) noexcept -> NativeScripts::ModuleInitContextBase
{
    FO_NO_STACK_TRACE_ENTRY();

    NativeScripts::ModuleInitContextBase ctx {};
    ctx.Engine = _engine.get();
    ctx.ScriptSys = _scriptSys.get();
    ctx.Meta = _meta.get();
    ctx.IsBaker = isBaker;
    return ctx;
}

FO_END_NAMESPACE

#endif
