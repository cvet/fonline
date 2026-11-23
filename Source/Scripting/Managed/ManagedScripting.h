// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_MANAGED_SCRIPTING

#include "EngineBase.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

// bake_output_dir: when non-empty, the managed backend also disk-loads assemblies from the bake output tree
// (<bake_output_dir>/<pack>/Assemblies/Assemblies-<target>, see MakeManagedAssemblyResourceDir)
void InitManagedScripting(ptr<EngineMetadata> meta, ptr<const FileSystem> resources, string_view assembly_cache_dir, string_view bake_output_dir = {});

FO_END_NAMESPACE

#endif
