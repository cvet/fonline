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

#include "ManagedScripting.h"

#if FO_MANAGED_SCRIPTING

#include "EngineBase.h"
#include "FileSystem.h"
#include "ManagedScriptBackend.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

void InitManagedScripting(EngineMetadata* meta, const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    auto managed_backend = SafeAlloc::MakeUnique<ManagedScriptBackend>();
    auto* managed_backend_ptr = managed_backend.get();

    if (auto* script_sys = dynamic_cast<ScriptSystem*>(meta)) {
        script_sys->RegisterBackend(ScriptSystemBackend::MANAGED_BACKEND_INDEX, std::move(managed_backend));
    }

    managed_backend_ptr->RegisterMetadata(meta);
    managed_backend_ptr->LoadAssemblies(resources);
    managed_backend_ptr->BindRequiredStuff();
}

FO_END_NAMESPACE

#endif
