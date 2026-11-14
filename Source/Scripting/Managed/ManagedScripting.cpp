// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ManagedScripting.h"

#if FO_MANAGED_SCRIPTING

#include "EngineBase.h"
#include "FileSystem.h"
#include "ManagedScriptBackend.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

void InitManagedScripting(ptr<EngineMetadata> meta, ptr<const FileSystem> resources, string_view assembly_cache_dir, string_view bake_output_dir)
{
    FO_TRACE_ZONE(Script);

    nptr<ScriptSystem> script_sys {meta.dyn_cast<ScriptSystem>()};
    FO_VERIFY_AND_THROW(script_sys, "Managed scripting requires a script system");

    auto managed_backend {safe_alloc::make_unique<ManagedScriptBackend>()};
    ptr<ManagedScriptBackend> backend {managed_backend};
    script_sys->RegisterBackend(ScriptSystemBackend::MANAGED_BACKEND_INDEX, std::move(managed_backend));

    backend->RegisterMetadata(meta);
    backend->LoadAssemblies(*resources, assembly_cache_dir, bake_output_dir);
    backend->BindRequiredStuff();
}

FO_END_NAMESPACE

#endif
