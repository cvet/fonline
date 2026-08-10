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

#include "AngelScriptScripting.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBackend.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ScriptSystem.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

struct AngelScriptAllocator
{
    static auto Alloc(size_t size) -> void*
    {
        FO_NO_STACK_TRACE_ENTRY();

        constexpr SafeAllocator<uint8_t> allocator;
        return allocator.allocate(size);
    }

    static void Free(void* raw_address)
    {
        FO_NO_STACK_TRACE_ENTRY();

        auto address = make_nptr(raw_address);

        if (!address) {
            return;
        }

        constexpr SafeAllocator<uint8_t> allocator;
        allocator.deallocate(address.reinterpret_as<uint8_t>().get(), 0);
    }
};

static void PrepareAngelScriptRuntime()
{
    FO_STACK_TRACE_ENTRY();

    static std::once_flag init_once;

    std::call_once(init_once, [] {
        AngelScript::asSetGlobalMemoryFunctions(&AngelScriptAllocator::Alloc, &AngelScriptAllocator::Free);

        int32_t prepare_result = AngelScript::asPrepareMultithread();
        FO_VERIFY_AND_THROW(prepare_result >= 0, "Prepare result is negative", prepare_result);
    });
}

void InitAngelScriptScripting(ptr<EngineMetadata> meta, const ScriptSettings& settings, const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    PrepareAngelScriptRuntime();

    auto as_backend_holder = SafeAlloc::MakeUnique<AngelScriptBackend>(&settings);
    auto as_backend = as_backend_holder.as_ptr();

    if (auto script_sys = meta.dyn_cast<ScriptSystem>()) {
        script_sys->RegisterBackend(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX, std::move(as_backend_holder));
    }

    as_backend->RegisterMetadata(meta);
    as_backend->LoadBinaryScripts(resources);
    as_backend->BindRequiredStuff();
}

auto CompileAngelScript(ptr<EngineMetadata> meta, const ScriptSettings& settings, const vector<File>& files, function<void(string_view)> message_callback) -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    PrepareAngelScriptRuntime();

    auto as_backend_holder = SafeAlloc::MakeUnique<AngelScriptBackend>(&settings);
    auto as_backend = as_backend_holder.as_ptr();

    if (auto script_sys = meta.dyn_cast<ScriptSystem>()) {
        script_sys->RegisterBackend(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX, std::move(as_backend_holder));
    }

    as_backend->SetMessageCallback(std::move(message_callback));
    as_backend->RegisterMetadata(meta);
    auto result = as_backend->CompileTextScripts(files);
    as_backend->BindRequiredStuff();
    return result;
}

FO_END_NAMESPACE

#endif
