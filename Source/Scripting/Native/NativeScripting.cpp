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

#include "NativeScripting.h"

#if FO_NATIVE_SCRIPTING

#include "NativeScriptBackend.h"

FO_BEGIN_NAMESPACE

void InitNativeScripting(ptr<EngineMetadata> meta, const ScriptSettings& settings, const FileSystem& resources, NativeScripts::Detail::RegisterModulesFn registerModules, bool isBaker)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(resources);

    auto native_backend = SafeAlloc::MakeUnique<NativeScriptBackend>(settings);
    ptr<NativeScriptBackend> native_backend_ptr = native_backend;

    if (nptr<ScriptSystem> script_sys = meta.dyn_cast<ScriptSystem>(); script_sys) {
        script_sys->RegisterBackend(ScriptSystemBackend::NATIVE_BACKEND_INDEX, std::move(native_backend));
    }

    native_backend_ptr->Attach(meta);

    // `registerModules` is the synth-emitted per-role dispatcher
    // (`RegisterNativeScriptModules_Server`, etc.). Each
    // Server/Client/Mapper startup and MasterBaker hand their own dispatcher
    // in so only the right module set runs.
    if (registerModules != nullptr) {
        registerModules(native_backend_ptr->MakeContext(isBaker));
    }
}

FO_END_NAMESPACE

#endif
