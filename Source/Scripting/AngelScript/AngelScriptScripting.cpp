// FOnline Engine
// SPDX-License-Identifier: MIT

#include "AngelScriptScripting.h"

#if FO_ANGELSCRIPT_SCRIPTING

#include "AngelScriptBackend.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "ScriptSystem.h"

#include <angelscript.h>

FO_BEGIN_NAMESPACE

namespace AngelScriptAllocator
{
    static auto Alloc(size_t size) -> void*
    {
        constexpr safe_allocator<uint8_t> allocator {};
        return allocator.allocate(size);
    }

    static void Free(void* raw_address)
    {
        auto address {make_nptr(raw_address)};

        if (!address) {
            return;
        }

        constexpr safe_allocator<uint8_t> allocator {};
        allocator.deallocate(address.reinterpret_as<uint8_t>().get(), 0);
    }
}

static void PrepareAngelScriptRuntime()
{
    static std::once_flag init_once {};

    std::call_once(init_once, [] {
        AngelScript::asSetGlobalMemoryFunctions(&AngelScriptAllocator::Alloc, &AngelScriptAllocator::Free);

        int32_t prepare_result {AngelScript::asPrepareMultithread()};
        FO_VERIFY_AND_THROW(prepare_result >= 0, "Prepare result is negative", prepare_result);
    });
}

void InitAngelScriptScripting(ptr<EngineMetadata> meta, const AngelScriptSettings& settings, const FileSystem& resources)
{
    FO_TRACE_ZONE(Script);

    PrepareAngelScriptRuntime();

    auto as_backend_holder {safe_alloc::make_unique<AngelScriptBackend>(&settings)};
    auto as_backend {as_backend_holder.as_ptr()};

    if (auto script_sys {meta.dyn_cast<ScriptSystem>()}) {
        script_sys->RegisterBackend(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX, std::move(as_backend_holder));
    }

    as_backend->RegisterMetadata(meta);
    as_backend->LoadBinaryScripts(resources);
    as_backend->BindRequiredStuff();
}

auto CompileAngelScript(ptr<EngineMetadata> meta, const AngelScriptSettings& settings, const vector<File>& files, function<void(string_view)> message_callback) -> vector<uint8_t>
{
    FO_TRACE_ZONE(Script);

    PrepareAngelScriptRuntime();

    auto as_backend_holder {safe_alloc::make_unique<AngelScriptBackend>(&settings)};
    auto as_backend {as_backend_holder.as_ptr()};

    if (auto script_sys {meta.dyn_cast<ScriptSystem>()}) {
        script_sys->RegisterBackend(ScriptSystemBackend::ANGELSCRIPT_BACKEND_INDEX, std::move(as_backend_holder));
    }

    as_backend->SetMessageCallback(std::move(message_callback));
    as_backend->RegisterMetadata(meta);
    auto result {as_backend->CompileTextScripts(files)};
    as_backend->BindRequiredStuff();
    return result;
}

FO_END_NAMESPACE

#endif
